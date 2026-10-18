"""按实际 SQL 与执行路径绘制书稿 ER 图和流程图，复用项目浅色配色。"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont
from overview_canvas import BG, PANEL, PANEL_EDGE, PTITLE, TITLE, NOTE, ARROW, FONT_SANS, FONT_SANS_INDEX

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'doc/book/images'
SCALE = 3


class Drawing:
    """在三倍画布上画固定布局，再降采样输出。"""
    def __init__(self, title, height=850):
        """创建带标题的浅色画布。"""
        self.image = Image.new('RGB', (1200*SCALE, height*SCALE), BG)
        self.draw = ImageDraw.Draw(self.image)
        self.text((36, 28), title, 32, TITLE)

    def text(self, position, text, size=22, color=NOTE):
        """绘制一段中文或代码文字。"""
        font = ImageFont.truetype(FONT_SANS, size*SCALE, index=FONT_SANS_INDEX)
        self.draw.text(tuple(int(x*SCALE) for x in position), text, font=font, fill=color)

    def box(self, xy, title, lines=()):
        """绘制实体表或流程节点。"""
        x,y,w,h=xy
        self.draw.rounded_rectangle((x*SCALE,y*SCALE,(x+w)*SCALE,(y+h)*SCALE),radius=14*SCALE,fill=PANEL,outline=PANEL_EDGE,width=2*SCALE)
        self.text((x+16,y+13),title,25,PTITLE)
        for index,line in enumerate(lines):self.text((x+16,y+54+index*31),line,21)

    def arrow(self, points, label=None, label_at=None):
        """绘制带箭头的折线路径与标签。"""
        scaled=[(int(x*SCALE),int(y*SCALE)) for x,y in points]
        self.draw.line(scaled,fill=ARROW,width=3*SCALE,joint='curve')
        for x,y in scaled:self.draw.ellipse((x-1.5*SCALE,y-1.5*SCALE,x+1.5*SCALE,y+1.5*SCALE),fill=ARROW)
        x,y=scaled[-1];px,py=scaled[-2];angle=math.atan2(y-py,x-px)
        self.draw.polygon([(x,y),(x-12*SCALE*math.cos(angle-.45),y-12*SCALE*math.sin(angle-.45)),(x-12*SCALE*math.cos(angle+.45),y-12*SCALE*math.sin(angle+.45))],fill=ARROW)
        if label:self.text(label_at,label,20,PTITLE)

    def save(self, name):
        """输出确定性的 PNG。"""
        OUTPUT.mkdir(parents=True,exist_ok=True)
        self.image.resize((self.image.width//SCALE,self.image.height//SCALE),Image.Resampling.LANCZOS).save(OUTPUT/name)


def entity_relationship():
    """按 fixture_ipage.sql 绘制外键、可空关系与删除行为。"""
    d=Drawing('学生信息系统：实际数据库 ER 图')
    d.box((40,120,350,210),'sclass · 班级',['PK  sclass_id','sclass_name','sclass_administrator','sclass_grade (可空)'])
    d.box((40,430,350,215),'student · 学生',['PK  student_id','student_name / student_sex','FK  sclass_id (可空)'])
    d.box((450,390,350,275),'selection · 成绩与选课',['PK  selection_id','FK  student_id (非空)','FK  course_id (非空)','test_date','grade_number'])
    d.box((850,120,310,210),'course · 课程',['PK  course_id','course_name','course_state'])
    d.box((850,450,310,215),'user · 认证用户',['PK  user_id','user_name','user_password','user_permission'])
    d.arrow([(210,330),(210,430)],'1 → 0..*；SET NULL',(230,362))
    d.arrow([(390,540),(450,540)])
    d.text((385,492),'1',19);d.text((414,558),'0..*',19)
    d.text((404,590),'CASCADE',17)
    d.arrow([(1005,330),(1005,355),(630,355),(630,390)],'1 → 0..*；CASCADE',(686,322))
    d.text((40,715),'每条成绩必须关联一个学生和一门课程；学生可以尚未分班。',22)
    d.text((40,754),'user 表没有指向 student 的外键，不把登录用户画成学生记录的一对一关系。',21)
    d.text((40,794),'来源：example/sql-magel/tests/fixture_ipage.sql；箭头从父表指向引用它的子表。',18)
    d.save('student-er.png')


def request_flow():
    """绘制前后端边界、预检、会话与后端权限判定。"""
    d=Drawing('学生系统：跨源请求与权限流程',1050)
    d.box((60,100,470,115),'浏览器：静态页面 :5500',['读取 API 地址，携带 Bearer 令牌'])
    d.box((60,275,470,115),'API :8080：解析与路由',['校验 HTTP；处理 CORS OPTIONS'])
    d.box((660,275,470,115),'OPTIONS 预检',['返回 CORS 头，不执行业务 SQL'])
    d.box((60,450,470,115),'公开接口 / 登录检查',['health 与 login 走公开分支'])
    d.box((660,450,470,115),'缺少或失效的会话',['受保护接口返回 401'])
    d.box((60,625,470,115),'后端权限与字段校验',['等级 ≥ 动作门槛；校验参数'])
    d.box((660,625,470,115),'拒绝执行',['越权 403；字段错误 400'])
    d.box((60,800,470,115),'数据库与 JSON 响应',['执行业务 SQL，返回状态与数据'])
    d.box((660,800,470,115),'浏览器渲染',['textContent 显示，菜单只作提示'])
    d.arrow([(295,215),(295,275)])
    d.arrow([(530,330),(660,330)],'预检',(560,297))
    d.arrow([(295,390),(295,450)],'业务请求',(320,408))
    d.arrow([(530,510),(660,510)],'未登录',(553,477))
    d.arrow([(60,510),(24,510),(24,857),(60,857)],'公开分支',(66,768))
    d.arrow([(295,565),(295,625)],'会话有效',(320,581))
    d.arrow([(530,682),(660,682)],'不满足',(553,649))
    d.arrow([(295,740),(295,800)],'检查通过',(320,756))
    d.arrow([(530,857),(660,857)],'跨源响应',(548,824))
    d.text((60,970),'公开接口在路由中单独处理；登录成功创建会话，前端隐藏按钮不替代后端授权。',21)
    d.save('student-request-flow.png')


def mcu_flow():
    """绘制实际板端菜单、加载与每轮执行的恢复路径。"""
    d=Drawing('H743 Magel 解释器：一轮程序的生命周期',1040)
    d.box((60,100,470,105),'上电、初始化与启动测速',['配置 SDRAM 内存池、外设与胶水'])
    d.box((60,265,470,105),'程序选择菜单',['内置程序 + FatFS 0:/magel/'])
    d.box((60,430,470,105),'加载三份映像',['.svmb / .data.bin / .layout'])
    d.box((660,430,470,105),'加载失败',['缺文件或映像错误：记录诊断'])
    d.box((60,595,470,105),'共享 VM 执行',['按键协作退出；检查指令预算'])
    d.box((660,595,470,105),'本轮结果',['正常返回 / panic / 预算超限'])
    d.box((660,790,470,105),'清理本轮并返回菜单',['释放程序、数据区、VM 与注册表'])
    d.arrow([(295,205),(295,265)])
    d.arrow([(295,370),(295,430)],'ENTER',(320,385))
    d.arrow([(530,482),(660,482)],'失败',(560,449))
    d.arrow([(295,535),(295,595)],'成功',(320,549))
    d.arrow([(530,647),(660,647)])
    d.arrow([(895,535),(895,570),(1147,570),(1147,840),(1130,840)])
    d.arrow([(895,700),(895,790)])
    d.arrow([(660,840),(40,840),(40,317),(60,317)],'重新选择',(70,802))
    d.text((60,952),'默认上电先自动运行一次 benchmark，再回到菜单。硬件 Fault 不属于语言 panic。',20)
    d.save('mcu-execution-flow.png')


if __name__=='__main__':
    entity_relationship()
    request_flow()
    mcu_flow()
