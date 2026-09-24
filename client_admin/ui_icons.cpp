#include "icon_assets.h"
#include <QPainter>
#include <QPainterPath>
namespace ncs::uiicons {
namespace {
QPixmap draw(Symbol symbol, const QColor &color, int size=24)
{
    QPixmap pixmap(size,size); pixmap.fill(Qt::transparent); QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(color,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
    switch(symbol){
    case Symbol::Analytics:p.drawLine(4,19,4,10);p.drawLine(10,19,10,6);p.drawLine(16,19,16,13);p.drawLine(21,19,21,4);p.drawLine(3,19,21,19);break;
    case Symbol::Status:p.drawEllipse(QRectF(3,3,18,18));p.drawArc(QRectF(7,9,10,10),0,180*16);p.drawLine(12,12,16,8);break;
    case Symbol::Charger:p.drawRoundedRect(QRectF(5,3,10,18),2,2);p.drawLine(8,7,12,7);p.drawLine(15,8,18,8);p.drawLine(18,8,18,16);p.drawLine(18,16,21,16);p.drawLine(9,12,12,12);p.drawLine(12,12,9,17);break;
    case Symbol::Station:{QPainterPath path;path.moveTo(12,22);path.cubicTo(5,16,5,12,5,9);path.arcTo(QRectF(5,2,14,14),180,-180);path.cubicTo(19,12,19,16,12,22);p.drawPath(path);p.drawEllipse(QRectF(9.5,6.5,5,5));break;}
    case Symbol::Users:p.drawEllipse(QRectF(6,4,6,6));p.drawArc(QRectF(3,13,12,9),0,180*16);p.drawArc(QRectF(13,5,6,6),-90*16,180*16);p.drawArc(QRectF(14,14,8,7),0,110*16);break;
    case Symbol::Prediction:p.drawPolyline(QPolygonF({{3,17},{8,12},{12,15},{20,6}}));p.drawLine(16,6,20,6);p.drawLine(20,6,20,10);p.drawLine(4,5,4,8);p.drawLine(2.5,6.5,5.5,6.5);break;
    case Symbol::Refresh:p.drawArc(QRectF(4,4,16,16),35*16,250*16);p.drawLine(19,4,19,9);p.drawLine(19,9,14,9);break;
    case Symbol::Logout:p.drawLine(10,4,5,4);p.drawLine(5,4,5,20);p.drawLine(5,20,10,20);p.drawLine(9,12,20,12);p.drawLine(16,8,20,12);p.drawLine(20,12,16,16);break;
    case Symbol::Help:p.drawEllipse(QRectF(3,3,18,18));p.drawArc(QRectF(8,6,8,7),0,210*16);p.drawPoint(12,17);break;
    case Symbol::Search:p.drawEllipse(QRectF(4,4,10,10));p.drawLine(13,13,19,19);break;
    case Symbol::Empty:p.drawRoundedRect(QRectF(4,5,16,15),3,3);p.drawLine(8,10,16,10);p.drawLine(8,14,14,14);break;
    }
    return pixmap;
}
}
QIcon icon(Symbol symbol,bool selectable){QIcon result;result.addPixmap(draw(symbol,selectable?QColor("#D4D4DA"):QColor("#5F5D66")),QIcon::Normal,QIcon::Off);if(selectable)result.addPixmap(draw(symbol,QColor("#29292E")),QIcon::Normal,QIcon::On);return result;}
QPixmap trendPixmap(){QPixmap result(96,40);result.fill(Qt::transparent);QPainter p(&result);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#8B80CB"),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));QPainterPath path;path.moveTo(3,31);path.cubicTo(15,30,18,18,30,19);path.cubicTo(41,20,43,27,54,24);path.cubicTo(67,21,71,8,93,6);p.drawPath(path);p.setPen(QPen(QColor("#D9D5E8"),1,Qt::DashLine));p.drawLine(3,35,93,35);return result;}
} // namespace ncs::uiicons

