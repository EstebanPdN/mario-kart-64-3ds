"""Full-resolution MK64 menu using owner artwork and pack-referenced AI sprites."""
from pathlib import Path
from PySide6.QtCore import QRectF, Qt, QPointF
from PySide6.QtGui import QImage, QPainter, QColor, QPen, QPolygonF
from game_font import GameFont


class MenuArt:
    @staticmethod
    def trim(image):
        alpha=image.convertToFormat(QImage.Format.Format_Alpha8)
        raw=bytes(alpha.constBits());stride=alpha.bytesPerLine()
        left,top,right,bottom=image.width(),image.height(),0,0
        for y in range(image.height()):
            row=raw[y*stride:y*stride+image.width()]
            visible=bytes(255 if value>96 else 0 for value in row)
            if visible.count(255)>=max(4,image.width()//150):
                left=min(left,len(visible)-len(visible.lstrip(b'\0')))
                right=max(right,len(visible.rstrip(b'\0')))
                top=min(top,y);bottom=y+1
        return image.copy(left,top,right-left,bottom-top) if right>left else image

    @staticmethod
    def sprite(painter,image,box):
        scale=min(box.width()/image.width(),box.height()/image.height())
        w,h=image.width()*scale,image.height()*scale
        painter.drawImage(QRectF(box.center().x()-w/2,box.center().y()-h/2,w,h),image)

    def __init__(self):
        root=Path(__file__).with_name('assets')
        self.logo=QImage(str(root/'logo-hd.png'))
        self.heading=QImage(str(root/'heading-pack.png'))
        self.actions=QImage(str(root/'actions-pack.png'))
        self.background=QImage(str(root/'background-loading.png'))
        if any(x.isNull() for x in (self.logo,self.heading,self.actions,self.background)):
            raise ValueError('Missing menu artwork')
        self.logo=self.trim(self.logo)
        self.heading=self.trim(self.heading)
        self.choose=self.trim(self.actions.copy(0,0,self.actions.width(),self.actions.height()//2))
        self.cancel=self.trim(self.actions.copy(0,self.actions.height()//2,self.actions.width(),self.actions.height()-self.actions.height()//2))
        self.font=GameFont()

    def render(self,phase='DROP ROM HERE',percent=0,busy=False,ready=False,filename='',cancelled=False,failed=False,focused=False):
        image=QImage(1280,1280,QImage.Format.Format_ARGB32)
        painter=QPainter(image)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing,True)
        painter.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform,True)
        painter.scale(4,4)
        # Exact background layer from the 3DS loading screen, including its
        # original RGB565 palette and baked attenuation. Only crop and scale.
        side=min(self.background.width(),self.background.height())
        crop=QRectF((self.background.width()-side)/2,(self.background.height()-side)/2,side,side)
        painter.drawImage(QRectF(0,0,320,320),self.background,crop)
        # All elements share one center line. Sprite aspect ratios are kept.
        self.sprite(painter,self.logo,QRectF(72,12,176,72))
        painter.fillRect(QRectF(28,99,264,36),QColor(0,0,0,225))
        self.sprite(painter,self.heading,QRectF(33,101,254,32))
        if busy:
            label='CANCELLING...' if phase=='CANCELLING' else 'CHECKING ROM...' if phase=='Checking ROM' else 'VERIFYING DATA...' if phase=='Verifying game data' else 'PREPARING GAME...'
            self.font.draw(painter,label,160,163,.72,280)
            painter.fillRect(QRectF(50,183,220,15),QColor('#000000'))
            painter.setPen(QPen(QColor('#879094'),.7));painter.drawRect(QRectF(50,183,220,15))
            if percent:
                painter.fillRect(QRectF(52,185,216*min(100,max(0,percent))/100,11),QColor('#f5ae0e'))
                painter.fillRect(QRectF(52,185,216*min(100,percent)/100,1.5),QColor('#ffed49'))
            self.font.draw(painter,f'{percent}%',160,190.5,.6)
            detail=filename or 'PLEASE KEEP THE DRIVE CONNECTED'
        elif ready:
            self.font.draw(painter,'GAME DATA IS READY!',160,163,.72,280)
            self.font.draw(painter,'COPY TO SD:/3DS/MK64/',160,190,.58,280)
            detail='CHOOSE A ROM TO PREPARE ANOTHER FILE'
        elif cancelled:
            self.font.draw(painter,'EXTRACTION CANCELLED',160,163,.68,280)
            self.font.draw(painter,'YOUR FILES WERE PRESERVED',160,190,.54,280)
            detail='USA ROM - .Z64 / .N64 / .V64'
        elif failed:
            self.font.draw(painter,'CHECK YOUR ROM',160,163,.72,280)
            self.font.draw(painter,'USA VERSION REQUIRED',160,190,.58,280)
            detail='USA ROM - .Z64 / .N64 / .V64'
        else:
            self.font.draw(painter,'DROP YOUR ROM HERE',160,163,.72,280)
            self.font.draw(painter,'OR',160,190,.58,280)
            detail='USA ROM - .Z64 / .N64 / .V64'
        self.sprite(painter,self.cancel if busy else self.choose,QRectF(66,218,188,38))
        self.font.draw(painter,detail,160,285,.48,288)
        if focused:
            painter.setPen(QPen(QColor('#ffffff'),.6,Qt.PenStyle.DotLine));painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawRect(QRectF(68,220,184,34))
        painter.end()
        return image
