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


def linux_flow():
    """绘制嵌入式 Linux 宿主的部署、执行和进程恢复路径。"""
    d=Drawing('嵌入式 Linux：字节码与宿主的部署流程',1040)
    d.box((60,100,470,105),'目标工具链构建宿主',['C++17、目标架构与系统运行库'])
    d.box((60,265,470,105),'部署宿主与程序资源',['共享 VM + 原生胶水 + 数据文件'])
    d.box((60,430,470,105),'加载与校验字节码',['.svmb / .data.bin / .layout'])
    d.box((660,430,470,105),'加载失败',['记录文件、格式和特征位诊断'])
    d.box((60,595,470,105),'共享 VM 执行',['检查内存、指令预算与宿主错误'])
    d.box((660,595,470,105),'执行结果',['正常返回 / panic / 预算超限'])
    d.box((660,790,470,105),'清理本轮并退出进程',['释放 VM、数据、注册表与资源'])
    d.arrow([(295,205),(295,265)])
    d.arrow([(295,370),(295,430)])
    d.arrow([(530,482),(660,482)],'失败',(560,449))
    d.arrow([(295,535),(295,595)],'成功',(320,549))
    d.arrow([(530,647),(660,647)])
    d.arrow([(895,535),(895,570),(1147,570),(1147,840),(1130,840)])
    d.arrow([(895,700),(895,790)])
    d.text((60,952),'由 Linux 服务管理器按退出状态决定是否重启；程序状态迁移需应用自行设计。',20)
    d.save('linux-execution-flow.png')


if __name__=='__main__':
    linux_flow()
