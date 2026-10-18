"""验证借用控制流中的释放、移动、分支、循环及多诊断。"""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/borrow-flow-tests"
PREFIX = "struct S:Drop{int64 value;void drop(){}};struct B{S left;S right;};S mut& identity(S mut& p){return p;}struct View{S left;S right;S mut& leftRef(){return &self.left;}S mut& rightRef(){S mut& r=&self.right;return r;}S mut& from(S mut& p){return p;}S mut& deadLeft(){if(false){return &self.left;}return &self.right;}S mut& earlyRight(bool flag){if(flag){return &self.right;}S mut& p=&self.left;p=&self.right;return p;}S mut& shadowRight(){S mut& p=&self.right;{S mut& p=&self.left;}return p;}S mut& assignedRight(){S mut& p=&self.left;{p=&self.right;}return p;}S mut& fresh(){S mut& p = heap S;return p;}};"


def main():
    """每个用例独立编译，保证诊断来自目标路径。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    compiler = parser.parse_args().magel.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    rejected = {
        "break_level_moved_after": "S s;loop{loop{S taken=std::lang::move(&s);break level 2;};};int64 x=s.value;",
        "break_all_moved_after": "S s;loop{loop{S taken=std::lang::move(&s);break all;};};int64 x=s.value;",
        "break_duplicate_move": 'auto result=loop{String source="x";yield (source,source);};',
        "capture_float_write": "float32 a=1;auto f=float32 fn move(){a};a=2;",
        "capture_float_read": "float32 a=1;auto f=float32 fn move(){a};float32 x=a;",
        "capture_reference_rebind": "int64 n=1;int64 mut& r=&n;auto f=int64 fn move[r](){r};r=&n;",
        "numeric_move": "int64 a=42;int64 b=std::lang::move(&a);a=0;std::lang::assert(a==0 && b==42);",
        "reinitialize": "S a;S taken=std::lang::move(&a);a=taken;int64 x=a.value;",
        "method_field": "View v;S mut& p=v.leftRef();S taken=std::lang::move(&v.left);int64 x=p.value;",
        "method_parameter": "View v;S mut& p = heap S;S mut& q=v.from(p);delete p;int64 x=q.value;",
        "method_heap": "View mut& v = heap View;S mut& p=v.leftRef();delete v;int64 x=p.value;",
        "option_payload": "S mut& p = heap S;Option<S mut&> opt=Option<S mut&>::Value(p);S mut& q=opt.unwrap();delete p;int64 x=q.value;",
        "array_alias": "int64 data[]=new[2];int64 mut& p=&data[0];delete data;int64 x=*p;",
        "old_alias": "S mut& p = heap S;S mut& q=p;delete p;p = heap nS;int64 x=q.value;delete p;",
        "returned_alias": "S mut& p = heap S;S mut& q=identity(p);delete p;int64 x=q.value;",
        "direct_free": "S mut& p = heap S;delete p;int64 x=p.value;",
        "alias_free": "S mut& p = heap S;S mut& q=p;delete p;int64 x=q.value;",
        "branch_free": "S mut& p = heap S;if(flag){delete p;}int64 x=p.value;",
        "loop_free": "S mut& p = heap S;while(flag){int64 x=p.value;delete p;}",
        "do_free": "S mut& p = heap S;do{int64 x=p.value;delete p;}while(flag);",
        "break_free": "S mut& p = heap S;while(flag){delete p;break;}int64 x=p.value;",
        "continue_free": "S mut& p = heap S;while(flag){delete p;continue;}int64 x=p.value;",
        "move_alias": "S s;S mut& q=&s;S taken=std::lang::move(&s);int64 x=q.value;",
        "move_branch": "S s;if(flag){S taken=std::lang::move(&s);}int64 x=s.value;",
        "drop_read": "S s;s.drop();int64 x=s.value;",
        "write_freed": "S mut& p = heap S;delete p;p.value=1;",
    }
    accepted = {
        "break_level_skips_read": "S s;loop{loop{S taken=std::lang::move(&s);break level 2;};int64 x=s.value;};",
        "break_all_skips_read": "S s;loop{loop{S taken=std::lang::move(&s);break all;};int64 x=s.value;};",
        "break_external_copy": "S source;S value=loop{yield (source);};int64 x=source.value;",
        "break_value_loop_escape": "S value={loop{S source;while(true){yield (source);}int64 x=source.value;};};",
        "break_destructure_reentry": 'for(int64 i=0;i<3;i++){String value=loop{auto(text,n)=("x",1);yield (text);};}',
        "capture_float_redeclare": "float32 a=1;auto f=float32 fn move(){a};float32 a=2;std::lang::assert(f()==1 && a==2);",
        "capture_new_type": "float32 a=1;auto f=float32 fn move(){a};int64 a=2;std::lang::assert(f()==1 && a==2);",
        "method_scoped_shadow": "View v;S mut& p=v.shadowRight();S taken=std::lang::move(&v.left);int64 x=p.value;",
        "method_scoped_assignment": "View v;S mut& p=v.assignedRight();S taken=std::lang::move(&v.left);int64 x=p.value;",
        "method_dead_return": "View v;S mut& p=v.deadLeft();S taken=std::lang::move(&v.left);int64 x=p.value;",
        "method_early_return": "View v;S mut& p=v.earlyRight(flag);S taken=std::lang::move(&v.left);int64 x=p.value;",
        "method_disjoint": "View v;S mut& p=v.rightRef();S taken=std::lang::move(&v.left);int64 x=p.value;",
        "method_fresh_loop": "View v;while(flag){S mut& p=v.fresh();p.value=1;delete p;}",
        "double_delete": "S mut& p = heap S;delete p;delete p;",
        "double_drop": "S s;s.drop();s.drop();",
        "last_use": "S mut& p = heap S;S mut& q=p;q.value=1;int64 x=q.value;delete p;",
        "independent": "S mut& p = heap S;S mut& q = heap S;delete p;q.value=1;delete q;",
        "branch_return": "S mut& p = heap S;if(flag){delete p;return;}int64 x=p.value;delete p;",
        "branch_rebind": "S a;S b;S mut& q=&a;if(flag){q=&b;}else{q=&b;}S taken=std::lang::move(&a);int64 x=q.value;",
        "dead_loop": "S mut& p = heap S;while(false){delete p;}int64 x=p.value;delete p;",
        "dead_if": "S mut& p = heap S;if(false){delete p;}int64 x=p.value;delete p;",
        "short_circuit": "S s;S taken=std::lang::move(&s);bool value=false && s.value==0;",
        "moved_heap_delete": "S mut& p = heap S;S taken=std::lang::move(p);delete p;",
        "redeclare": "S a;S taken=std::lang::move(&a);S a;int64 x=a.value;",
        "redeclare_both_branches": "S a;if(flag){S taken=std::lang::move(&a);}else{S taken=std::lang::move(&a);}S a;int64 x=a.value;",
        "redeclare_return": "S a;if(flag){return;}else{S taken=std::lang::move(&a);}S a;int64 x=a.value;",
        "redeclare_alias_move": "S a;S mut& p=&a;S taken=std::lang::move(p);S a;int64 x=a.value;",
        "redeclare_scalar": "int64 a=42;int64 b=std::lang::move(&a);int64 a=7;",
        "shadow": "S a;S taken=std::lang::move(&a);{S a;int64 x=a.value;}",
        "array_loop": "while(flag){int64 data[]=new[2];data[0]=1;delete data;}",
        "heap_loop": "while(flag){S mut& p = heap S;p.value=1;delete p;}",
        "heap_reallocate": "S mut& p = heap S;delete p;p = heap nS;p.value=1;delete p;",
        "fresh_loop": "while(flag){S s;S taken=std::lang::move(&s);}",
        "disjoint_field": "B b;S taken=std::lang::move(&b.left);int64 x=b.right.value;",
    }
    env = dict(os.environ, MAGEL_LANG="zh")
    for reject, cases in ((True, rejected), (False, accepted)):
        for name, body in cases.items():
            path = WORK / (name + ".mgl")
            path.write_text(PREFIX + "void test(bool flag){" + body + "}int64 main(){return 0;}\n", encoding="utf-8")
            result = subprocess.run([str(compiler), "--diagnostics=json", str(path)], cwd=ROOT,
                                    env=env, capture_output=True, encoding="utf-8", timeout=120)
            assert bool(result.returncode) == reject, (name, result.returncode, result.stdout, result.stderr)
            payload = json.loads(result.stdout)
            if reject:
                assert any(d["code"] == "MGL1205" for d in payload["diagnostics"]), (name, payload)
            else:
                assert not payload["diagnostics"], (name, payload)
    path = WORK / "multiple.mgl"
    path.write_text(PREFIX + "void test(){\nS mut& p = heap nS;delete p;\nint64 a=p.value;\nint64 b=p.value;\n}int64 main(){return 0;}\n", encoding="utf-8")
    result = subprocess.run([str(compiler), "--diagnostics=json", str(path)], cwd=ROOT, env=env,
                            capture_output=True, encoding="utf-8", timeout=120)
    diagnostics = json.loads(result.stdout)["diagnostics"]
    assert result.returncode and len(diagnostics) == 2, (result, diagnostics)
    assert {item["span"]["line"] for item in diagnostics} == {3, 4}, diagnostics
    print(f"借用控制流：{len(rejected)} 项拒绝、{len(accepted)} 项允许、多诊断通过")


if __name__ == "__main__":
    main()
