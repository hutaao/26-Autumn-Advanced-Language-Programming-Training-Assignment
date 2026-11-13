#include <QtWidgets>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif
using P = QPointF;
const QColor bone("#e9e7db"), mint("#93d9c5"), amber("#e7b477"), muted("#87908f");
double length(P v){return std::hypot(v.x(),v.y());}
P normalized(P v){return v/std::max(.0001,length(v));}
double segmentDistance(P p,QLineF s){
    P delta=s.p2()-s.p1();
    double u=std::clamp(P::dotProduct(p-s.p1(),delta)/std::max(.001,P::dotProduct(delta,delta)),0.,1.);
    return length(p-s.p1()-u*delta);
}
class Training : public QWidget {
    enum Lesson { Move, Dash, Sheathe, Echo, Lamp, Complete };
    Lesson lesson=Move;
    const P marker{300,340},dummy{448,340},receiver{746,340};
    const QRectF column{698,204,50,32};
    P hero{145,340}, pointer{300,340}, velocity, lamp{535,159}, slashStart;
    QVector<QLineF> cracks, flashes;
    QLineF retiringCrack;
    double retiredAt=-20,hurtAt=-20;
    int loseHeart(){hurtAt=time;play(2);return --hearts;}
    QSet<int> held;
    QTimer timer; QElapsedTimer elapsed;
    double time=0,dashLeft=0,cooldown=0,flash=0,settle=0,hit=0,walkPhase=0;
    bool dragging=false,mute=false;
    QString hint;double hintTime=0;
    QVector<QByteArray> tones;
    void prepareTones(){
        for(int n=0;n<4;++n){
            constexpr int rate=22050;
            const int samples=n==0?15000:6000;
            QByteArray wave;
            QDataStream s(&wave,QIODevice::WriteOnly);
            s.setByteOrder(QDataStream::LittleEndian);
            s.writeRawData("RIFF",4);s<<quint32(36+2*samples);s.writeRawData("WAVEfmt ",8);
            s<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(rate)<<quint32(2*rate)<<quint16(2)<<quint16(16);
            s.writeRawData("data",4);s<<quint32(2*samples);
            for(int j=0;j<samples;++j){
                double x=double(j)/rate;
                double f=n==0?760:n==1?170:n==2?245:520;
                double v=std::sin(6.28318*(f*x-60*x*x))*std::exp(-x*(n==0?7:15));
                if(n==0&&x>.16)v+=.3*std::sin(6.28318*f*(x-.16))*std::exp(-8*(x-.16));
                s<<qint16(std::min(1.,x*160)*v*6100);
            }
            tones.push_back(wave);
        }
    }
    void play(int n){
#ifdef Q_OS_WIN
        if(!mute)PlaySoundA(tones[n].constData(),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
#else
        Q_UNUSED(n);
#endif
    }

    double scaleFactor()const{return std::min(width()/1000.,height()/615.);}
    QRectF field()const{
        const QSizeF size(width()/scaleFactor(),height()/scaleFactor());
        return QRectF(P(500,307.5)-P(size.width()/2,size.height()/2),size);
    }
    struct Guard {
        P home,pos,attack;
        double phase=0, timer=0;
        int state=0; double hearsAt=1e9, lastAudible=-20; P heardFrom; int hp=2; bool armored=false; double heat=0, deflected=-20; bool fast=false,shadowbound=false; double mapAt=-100;P mapContact; // Patrol, wind-up, lunge, recover, defeated.
    };
    QVector<Guard> guards;
    bool chapter=false,finished=false;
    double camera=0,arrival=0,immunity=0;
    P stageLamp{805,114}, impactPoint;
    const QRectF pier{1175,206,80,26};
    P exitPoint{1685,268};
    const QVector<QRectF> baffles={{554,200,15,120},{638,296,15,134},{724,200,15,125}};
    bool scanned=false,facesLeft=false;
    P waveOrigin, safeSpot;
    double waveStart=-20;
    bool lampHandled=false, exposureArmed=false;

    int sector=-1;
    struct Relay {P pos;bool seen=false,cut=false;double arrival=1e9,charge=0,released=-20;};
    QVector<Relay> relays;
    QVector<QRectF> stacks;
    bool sealed()const{for(auto r:relays)if(!r.cut)return true;return false;}
    struct Channel { QRectF pit; double crossing; };
    QVector<Channel> channels()const{
        if(sector>=0)return {};
        return {{{1148,98,134,420},340}};
    }
    P safeBank{1119,340};
    bool stageCombatRequired()const{return sector!=2;}
    struct Projector{
        P source;
        QRectF post;
        QColor color;
        double left,right;
        QPainterPath beam,umbra;
    };
    QVector<Projector> projectors;
    int draggedProjector=-1;
    QRectF chapterBounds()const{return lowerBounds;}
    static void rebuildProjection(Projector &light){
        P axis=normalized(light.post.center()-light.source),side(-axis.y(),axis.x());
        P lens=light.source+axis*13;
        const double reach=555,farHalf=9+reach*.44523;
        QPainterPath beam;
        beam.addPolygon({lens-side*9,lens+side*9,lens+axis*reach+side*farHalf,lens+axis*reach-side*farHalf});
        beam.closeSubpath();light.beam=beam;
        QRectF p=light.post;QPainterPath shade;
        for(QLineF edge:{QLineF(p.topLeft(),p.topRight()),QLineF(p.topRight(),p.bottomRight()),QLineF(p.bottomRight(),p.bottomLeft()),QLineF(p.bottomLeft(),p.topLeft())}){
            QPainterPath projected;
            projected.addPolygon({edge.p1(),edge.p2(),lens+(edge.p2()-lens)*20,lens+(edge.p1()-lens)*20});
            shade=shade.united(projected);
        }
        light.umbra=beam.intersected(shade);light.beam=beam.subtracted(shade);
    }
    void initializeProjectors(){
        projectors.clear();draggedProjector=-1;
        if(sector!=2)return;
        auto mount=[&](double x,double y,double dx,QColor color){
            projectors.push_back({{x+dx,114},QRectF(x-36,y-12,72,24),color,x-155,x+155,{},{}});
            rebuildProjection(projectors.last());
        };
        mount(1175,220,38,QColor("#78dfed"));
        mount(1325,220,-45,QColor("#f194cb"));
        for(auto &g:projectors){g.post.setWidth(22);g.post.setHeight(14);
            g.post.moveCenter({(g.left+g.right)/2,220});rebuildProjection(g);}
    }
    bool primaryLit(P p)const{
        return sector!=2&&projectedLight().contains(p)&&!castShade(p);
    }
    bool litAt(P p)const{
        if(primaryLit(p))return true;
        for(const auto &light:projectors)if(light.beam.contains(p))return true;
        return false;
    }
    // Etched gates at opposite ends of each diagonal must both be in
    // its column shadow. The central lozenge requires the actual overlap.
    P sealMark(int lamp,int end)const{
        return P(1250,355)+P((lamp==0?1:-1)*end*24,end*43);
    }
    bool alignedShadow(int i)const{
        const auto &shade=projectors[i].umbra;
        return shade.contains(sealMark(i,-1))&&shade.contains(sealMark(i,1));
    }
    void drawSeal(QPainter &p){
        const P c=relays.first().pos;
        QPainterPath disc;disc.addEllipse(c,79,79);
        p.fillPath(disc,QColor("#101321"));ring(p,c,80,QColor("#384052"),2);
        for(int i=0;i<2;++i){
            const auto &lamp=projectors[i];QColor tint=lamp.color;tint.setAlpha(88);
            p.fillPath(disc.intersected(lamp.umbra),tint);
            if(!relays.first().seen)continue;
            P a=sealMark(i,-1),b=sealMark(i,1);
            p.setPen(QPen(lamp.color,1,Qt::DotLine));p.drawLine(a,b);
            for(P v:{a,b}){ring(p,v,6,lamp.color,1.5);
                if(lamp.umbra.contains(v)){p.setPen(Qt::NoPen);p.setBrush(lamp.color);p.drawEllipse(v,3,3);}}
        }
        p.fillPath(disc.intersected(projectors[0].umbra).intersected(projectors[1].umbra),QColor(239,224,255,135));
        if(relays.first().seen)for(int i=0;i<2;++i){
            QColor c1=alignedShadow(i)?projectors[i].color:QColor("#404758");
            p.setPen(QPen(c1,3));p.setBrush(Qt::NoBrush);p.drawArc(disc.boundingRect(),(i*180+8)*16,164*16);
        }
    }
    bool bridgeShade(P p)const{return castShade(p);}
    bool powered(const Relay&r)const{
        if(sector==2)return alignedShadow(0)&&alignedShadow(1)&&projectors[0].umbra.contains(r.pos)&&projectors[1].umbra.contains(r.pos);
        return litAt(r.pos);
    }
    void drawProjectors(QPainter &p){
        for(const Projector &g:projectors){
            p.save();p.setClipRect(chapterBounds(),Qt::IntersectClip);
            QColor tint=g.color;tint.setAlpha(63);
            QRadialGradient glow(g.source,580);glow.setColorAt(0,tint);tint.setAlpha(8);glow.setColorAt(1,tint);
            p.fillPath(g.beam,glow);p.restore();
            p.fillRect(g.post,QColor("#232332"));
            line(p,g.post.topLeft(),g.post.topRight(),g.color,2);
            line(p,g.post.bottomLeft(),g.post.bottomRight(),g.color.darker(215));
            line(p,{g.left,114},{g.right,114},g.color.darker(280),3);
            for(int i=0;i<4;++i)line(p,{g.left+i*8,112},{g.left+i*8,119},g.color.darker(180));
            p.save();p.translate(g.source);P axis=normalized(g.post.center()-g.source);
            p.rotate(std::atan2(axis.y(),axis.x())*57.29578-90);
            shape(p,{{-12,-9},{7,-14},{14,-4},{11,11},{-11,11}},QColor("#242035"),g.color);
            line(p,{-9,11},{9,11},g.color,3);p.restore();
        }
        drawSeal(p);
    }
#include "warden.inc"
    const QStringList rooms={"盲廊","折返耳室","合影厅","交锋环廊","蚀光之巢"};
    const QVector<P> eastWing={{1400,255},{1470,395},{1540,322}};
    P resonance;
    QRectF shutter;
    bool passageFound=false;
    double resonanceAt=1e9,openedAt=-20;
    void layoutBranches(int n){
        // Every vestibule has a blind turn. The eastern partition can only
        // open from the resonant alcove on the near side.
        const double openings[]={170,455,440,172,425};
        const double partitions[]={1450,1270,1450,1310,1115};
        const P niches[]={{1240,455},{1060,155},{1040,165},{1070,455},{986,156}};
        const double x=partitions[n],y=openings[n];
        resonance=niches[n];passageFound=false;resonanceAt=1e9;openedAt=-20;
        switch(n){
        case 0: // The southern archive is entered from its western end.
            stacks.append(QRectF(1180,399,146,15));stacks.append(QRectF(1312,412,14,72));break;
        case 1: // Hook around the shelf into the northern listening pocket.
            stacks.append(QRectF(1007,198,106,15));stacks.append(QRectF(1007,130,14,68));break;
        case 2: // The side gallery opens east towards the seal, then loops back.
            stacks.append(QRectF(988,212,95,14));break;
        case 3: // Low partitions separate the southern alcove from the combat floor.
            stacks.append(QRectF(979,407,128,14));stacks.append(QRectF(1093,421,14,42));break;
        case 4: // A narrow service passage overlooks the arena entrance.
            stacks.append(QRectF(953,204,79,14));break;
        }
        stacks.append(QRectF(907,n==0||n==2||n==4?99:226,20,n==0||n==2||n==4?288:289));
        stacks.append(QRectF(x,99,18,y-36-99));
        stacks.append(QRectF(x,y+36,18,515-y-36));
        shutter={x,y-36,18,72};stacks.append(shutter);
    }
    int wave=0;double reinforcementsAt=0;
    void addGuard(P pos,bool fast=false){
        guards.push_back({pos,pos,{}});guards.last().fast=fast;
    }
    void beginSector(int n,bool fresh){

        int hp=hearts;sector=n;startChapter(true);calibration=false;camera=570;
        hearts=fresh?3:hp;relays.clear();stacks.clear();guards.clear();
        boss=Warden{};wave=0;reinforcementsAt=-1;medkit=n==1||n==3;safeBank=hero;initializeProjectors();if(n==2)camera=730;
        if(n==0){
            // A folded listening gallery: turn south, then return through the quiet upper aisle.
            stacks={{939,99,26,232},{939,333,105,22},{1093,200,23,308},
                    {1190,265,145,26},{1333,193,25,98},{1530,370,104,24}};
            addGuard({1052,470});addGuard({1372,149},true);addGuard({1540,315});
            relays={{{1044,419}}};exitPoint={1660,181};
        }else if(n==1){
            // The central barricade separates the guard from the southern listening aisle.
            stacks={{948,240,138,24},{989,376,26,106},{1281,344,112,22},
                    {1384,186,22,114},{1458,329,132,22},{1593,329,23,104}};
            relays={{{1515,410}}};exitPoint={1683,218};
            guards.push_back({{1472,428},{1472,428},{}});guards.last().armored=true;
            addGuard({1080,470},true);addGuard({1640,231});
        }else if(n==3){
            stacks={{1135,191,35,95},{1260,367,38,108},{1475,187,32,90}};
            stageLamp.setX(1160);kitPosition={1210,330};exitPoint={1660,329};
            addGuard({1040,301},true);addGuard({1115,360});
        }else if(n==4){
            medkit=true;kitPosition={870,405};stageLamp={1300,495};exitPoint={1650,343};camera=740;
        }else{
            stacks={{959,99,28,157},{959,389,28,126},{1460,245,32,130}};
            relays={{{1250,355}}};exitPoint={1612,355};
            guards.push_back({{1498,415},{1498,415},{}});
        }
        layoutBranches(n);
    }
    bool clearGuardLine(P from,P to)const{
        for(QRectF r:stacks)for(QLineF side:{QLineF(r.topLeft(),r.topRight()),QLineF(r.topRight(),r.bottomRight()),QLineF(r.bottomRight(),r.bottomLeft()),QLineF(r.bottomLeft(),r.topLeft())})
            if(QLineF(from,to).intersects(side,nullptr)==QLineF::BoundedIntersection)return false;
        return true;
    }
    void pulse(){
        if(lesson<Echo||finished||time-waveStart<1.15)return;
        waveOrigin=hero;waveStart=time;traceSound();
        auto arrivalTime=[&](P pos,const Leg &leg){
            double offset=std::clamp(P::dotProduct(pos-leg.path.p1(),normalized(leg.path.p2()-leg.path.p1())),0.,leg.path.length());
            return time+(leg.start+offset)/340.;
        };
        for(auto &g:guards)if(sector>=0&&hero.x()>935&&g.state!=4){
            for(const Leg &leg:rays)if(segmentDistance(g.pos,leg.path)<12){
                double at=arrivalTime(g.pos,leg);
                g.mapAt=at;g.mapContact=g.pos;
                if(g.state==0&&at<g.hearsAt){g.hearsAt=at;g.heardFrom=normalized(leg.path.p1()-leg.path.p2());}
            }
        }
        if(sector>=0&&!passageFound&&length(hero-resonance)<102){
            for(const Leg &leg:rays)if(segmentDistance(resonance,leg.path)<8)
                resonanceAt=std::min(resonanceAt,arrivalTime(resonance,leg));
        }
        recordContacts();
        for(auto &r:relays)if(!r.seen)for(const Leg &leg:rays){
            if((sector!=1||leg.reflection>0)&&segmentDistance(r.pos,leg.path)<9)
                r.arrival=std::min(r.arrival,arrivalTime(r.pos,leg));
        }
        playEcho();
        if(lesson==Echo && length(hero-P(648,320))<360)scanned=true;
    }
    struct Leg {QLineF path; double start; int wall,ray,reflection;quint32 history;};
    QVector<Leg> rays;
    QByteArray echoAudio;
    void playEcho(){
#ifdef Q_OS_WIN
        if(mute)return;
        // The first wall on each side returns a soft stereo tap at the visual contact time.
        double nearest[2]={1000,1000};
        for(const Leg &leg:rays)if(leg.reflection==0&&leg.wall>=0){
            int side=leg.path.dx()>0?1:0;nearest[side]=std::min(nearest[side],leg.path.length());
        }
        QByteArray next;QDataStream out(&next,QIODevice::WriteOnly);out.setByteOrder(QDataStream::LittleEndian);
        constexpr int rate=16000,frames=15800;
        out.writeRawData("RIFF",4);out<<quint32(36+4*frames);out.writeRawData("WAVEfmt ",8);
        out<<quint32(16)<<quint16(1)<<quint16(2)<<quint32(rate)<<quint32(rate*4)<<quint16(4)<<quint16(16);
        out.writeRawData("data",4);out<<quint32(frames*4);
        auto tap=[](double t,double pitch){
            if(t<0||t>.16)return 0.;
            return std::min(1.,180*t)*std::exp(-32*t)*std::sin(6.28318*(pitch*t-390*t*t));
        };
        for(int i=0;i<frames;++i){
            double t=double(i)/rate;
            for(int side:{0,1}){
                double send=tap(t,870)*.64+tap(t-.056,810)*.32+tap(t-.112,760)*.19;
                double back=tap(t-.032-nearest[side]/340.,545)*.65+tap(t-.039-nearest[1-side]/340.,545)*.16;
                out<<qint16((send+back)*7500);
            }
        }
        PlaySoundA(next.constData(),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
        echoAudio.swap(next);
#endif
    }
    void traceSound(){
        rays.clear();QVector<QLineF> edges;
        auto box=[&](QRectF r){edges<<QLineF(r.topLeft(),r.topRight())<<QLineF(r.topRight(),r.bottomRight())<<QLineF(r.bottomRight(),r.bottomLeft())<<QLineF(r.bottomLeft(),r.topLeft());};
        box(chapter?chapterBounds():upperBounds);for(auto &g:projectors)box(g.post);
        for(auto r:stacks)box(r);if(chapter){if(sector!=2)box(pier);for(auto c:channels())box(c.pit);}
        else{for(auto r:baffles)box(r);edges<<QLineF(521,205,798,205)<<QLineF(521,421,798,421);}
        for(int i=0;i<640;++i){
            double angle=i*6.2831853/640;P d(std::cos(angle),std::sin(angle)),o=waveOrigin;
            double traveled=0;quint32 history=0;
            for(int bounce=0;bounce<4 && traveled<268;++bounce){
                QLineF ray(o,o+d*(268-traveled));double nearest=ray.length();P end=ray.p2();int wall=-1;
                for(int j=0;j<edges.size();++j){P q;if(ray.intersects(edges[j],&q)!=QLineF::BoundedIntersection)continue;
                    double n=length(q-o);if(n>.001&&n<nearest){nearest=n;end=q;wall=j;}}
                rays.push_back({QLineF(o,end),traveled,wall,i,bounce,history});if(wall<0)break;
                history=history*97u+quint32(wall+1);
                if(edges[wall].dx()==0)d.setX(-d.x());else d.setY(-d.y());
                traveled+=nearest+.01;o=end+d*.01;
            }
        }
    }
    // Store only strips where neighbouring sound rays actually touched the same face.
    // A new scan can overlap the last one without erasing its recently heard traces.
    struct HeardStrip { QLineF edge; double arrives; int bounce; };
    QVector<HeardStrip> heard;
    void recordContacts(){
        heard.erase(std::remove_if(heard.begin(),heard.end(),[this](const HeardStrip &e){
            return time-e.arrives>15.;
        }),heard.end());
        struct End { P at; int ray=-2; double distance=0; };
        QHash<int,End> last;
        for(const Leg &leg:rays){
            if(leg.wall<0)continue;
            int key=leg.wall*4+leg.reflection;
            End end{leg.path.p2(),leg.ray,leg.start+leg.path.length()};
            End before=last.value(key);
            if(before.ray+1==end.ray && length(before.at-end.at)<12){
                heard.push_back({QLineF(before.at,end.at),
                    time+std::max(before.distance,end.distance)/340.,leg.reflection});
            }
            last.insert(key,end);
        }
    }
    void paintEcho(QPainter&p){
        for(const HeardStrip &strip:heard){
            double age=time-strip.arrives;
            if(age<0||age>=4.2)continue;
            double energy=std::exp(-age*.85);
            QColor c=strip.bounce?QColor("#afa6ff"):QColor("#85ebe9");
            c.setAlphaF(std::min(1.,energy*.92));
            line(p,strip.edge.p1(),strip.edge.p2(),c,1.7);
        }
        double travel=(time-waveStart)*340;if(travel>310)return;
        for(int pulse=0;pulse<3;++pulse){
            const double radius=travel-19*pulse;
            P last;int index=-2;quint32 history=0;
            for(const Leg &leg:rays){
                double n=leg.path.length(),along=radius-leg.start;
                if(n<.01||along<0||along>n)continue;
                P q=leg.path.pointAt(along/n);
                if(leg.ray==index+1&&history==leg.history&&length(q-last)<8){
                    QColor color=leg.reflection?QColor(175,184,255):QColor(132,225,228);
                    color.setAlpha(190-pulse*42);
                    line(p,last,q,color,1.6-pulse*.2);
                }
                last=q;index=leg.ray;history=leg.history;
            }
        }

    }
    bool calibration=false,medkit=false;
    double damageAt=-50,healedAt=-50;
    P kitPosition{1304,340};
    void startChapter(bool retry){
        projectors.clear();draggedProjector=-1;
        exitPoint={1685,268};kitPosition=sector==1?P(1720,475):P(1304,340);safeBank={1119,340};calibration=!retry;medkit=true;damageAt=healedAt=-50;
        heard.clear();rays.clear();waveStart=-20;exposure=0;chapter=true;finished=false;lesson=Complete;arrival=time;dragging=false;draggedProjector=-1;unsetCursor();
        hurtAt=retiredAt=-20;cracks.clear();flashes.clear();dashLeft=flash=hit=cooldown=0; bufferedCut=false;
        stageLamp={805,114};guards.clear();
        for(P at:{P(1410,333),P(1535,315)})guards.push_back({at,at,{},double(guards.size())*2.4});
        for(auto &g:guards)g.hp=1;
        if(retry){hero={818,340};hearts=3;exposure=0;held.clear();}
        if(sector<0){stageLamp.setX(1100);relays={{{1055,190},true}};}
        immunity=.8;
    }
    P beamDirection()const{if(sector==4)return boss.lightDir;return normalized(pier.center()-stageLamp);}
    P lensPosition()const{return stageLamp+beamDirection()*13;}
    QPainterPath projectedLight()const{
        if(sector==4&&lensBlocked())return {};
        P origin=lensPosition(),axis=beamDirection(),normal(-axis.y(),axis.x());
        QPainterPath fan;fan.addPolygon({origin-normal*9,origin+normal*9,
            origin+axis*1550+normal*(9+1550*std::tan((sector==4?5.5:24)*3.141592653589793/180)),origin+axis*1550-normal*(9+1550*std::tan((sector==4?5.5:24)*3.141592653589793/180))});fan.closeSubpath();return fan;
    }
    bool castShade(P point)const{
        QLineF ray(lensPosition(),point);
        for(auto side:{QLineF(pier.topLeft(),pier.topRight()),QLineF(pier.topRight(),pier.bottomRight()),QLineF(pier.bottomRight(),pier.bottomLeft()),QLineF(pier.bottomLeft(),pier.topLeft())})
            if(ray.intersects(side,nullptr)==QLineF::BoundedIntersection)return true;
        return false;
    }
    bool overWell(P pos)const{for(auto c:channels())if(c.pit.contains(pos))return true;return false;}
    const QRectF upperBounds{35,88,789,432},lowerBounds{769,99,980,416};
    bool chapterFloor(P pos)const{
        for(auto r:stacks)if(r.adjusted(-6,-6,6,6).contains(pos))return false;
        if(sector<0&&sealed()&&overWell(pos))return false;
        if(!chapterBounds().adjusted(7,7,-7,-7).contains(pos))return false;
        if(sector!=2&&pier.adjusted(-8,-6,8,6).contains(pos))return false;
        for(const Projector &g:projectors)if(g.post.adjusted(-7,-7,7,7).contains(pos))return false;
        for(auto c:channels())if(c.pit.contains(pos))return std::abs(pos.y()-c.crossing)<26&&bridgeShade(pos);return true;
    }
    void moveGuardTo(Guard &guard, P target){
        const P delta=target-guard.pos;
        const int steps=1+int(length(delta)/4);
        const P step=delta/steps;
        for(int i=0;i<steps;++i){
            const P next=guard.pos+step;
            if(!chapterFloor(next))break;
            guard.pos=next;
        }
    }
    bool cleared()const{return (sector!=4||boss.hp==0)&&std::all_of(guards.begin(),guards.end(),[](const Guard&g){return g.state==4;});}
    bool doorOpen()const{
        return (sector<0||passageFound)&&(sector!=3||wave==1)&&(!stageCombatRequired()||cleared())&&!sealed();
    }
    void runChapter(double dt){
        double target=std::min(765.,hero.x()-std::clamp(220.+(hero.x()-800.)*.65,220.,830.));
        if(sector==2)target=std::clamp(hero.x()-480.,730.,765.);
        camera+=(target-camera)*(1-std::exp(-dt*4));
        immunity=std::max(0.,immunity-dt);
        if(!overWell(hero)&&chapterFloor(hero))safeBank=hero;
        if(overWell(hero)&&!chapterFloor(hero)){hero=safeBank;dashLeft=0;}
        if(medkit && length(hero-kitPosition)<20 && hearts<3){
            medkit=false;++hearts;healedAt=time;play(3);
        }
        for(auto &r:relays){
            if(r.arrival<=time)r.seen=true;
            if(!r.seen||r.cut||r.released>=0||calibration)continue;
            r.charge=std::clamp(r.charge+dt*(powered(r)?1.0:-2.0),0.,1.35);
            if(r.charge>=1.35){r.released=time;play(3);}
        }

        if(sector==4)stepWarden(dt);
        if(sector==3&&wave==0&&cleared()){
            if(reinforcementsAt<0&&hero.x()>1160)reinforcementsAt=time+1.2;
            if(reinforcementsAt>=0&&time>=reinforcementsAt){
                wave=1;for(int i=0;i<eastWing.size();++i)addGuard(eastWing[i],i!=1);guards[3].shadowbound=true;play(1);
            }
        }
        if(hearts<=0){beginSector(sector,true);return;}
        bool died=false;
        for(auto &g:guards){
            if((sector>=0&&hero.x()<935)||g.state==4||(sector<0&&hero.x()<1282))continue;
            if(g.armored){
                const bool illuminated=litAt(g.pos);
                g.heat=illuminated?std::min(1.25,g.heat+dt):0.;
                if(g.heat>=1.25){g.armored=false;g.timer=1.15;g.state=3;play(3);}
            }
            if(g.hearsAt<=time){g.lastAudible=time;
                if(g.state==0){g.attack=g.heardFrom;g.timer=.92;g.state=1;}
                g.hearsAt=1e9;
            }
            if(g.state==0){g.phase+=dt;
                P patrol=(sector==1)?P(0,std::sin(g.phase*.7)*38):
                    (sector==2&&g.home.x()>1500)?P(35*std::sin(g.phase*.8),0):P(0,std::sin(g.phase)*35);
                moveGuardTo(g,g.home+patrol);
                if(length(hero-g.pos)<117&&clearGuardLine(g.pos,hero)){g.attack=normalized(hero-g.pos);g.state=1;g.timer=g.fast?.43:.65;}}
            else if(g.state==1){if((g.timer-=dt)<=0){g.state=2;g.timer=.27;}}
            else if(g.state==2){P next=g.pos+g.attack*(dt*(g.fast?440:320));if(chapterFloor(next)&&(sector>=0||next.x()>1285))g.pos=next;
                if(length(hero-g.pos)<22 && immunity<=0 && dashLeft<=0){loseHeart();immunity=1.3;died=hearts<=0;}
                if((g.timer-=dt)<=0){g.state=3;g.timer=1.2;}}
            else{moveGuardTo(g,g.pos+(g.home-g.pos)*(1-std::exp(-dt*4)));if((g.timer-=dt)<=0)g.state=0;}
        }
        if(died){if(sector>=0)beginSector(sector,true);else startChapter(true);return;}
        if(doorOpen()&&length(hero-exitPoint)<20){if(sector>=0&&sector<4){beginSector(sector+1,false);return;}finished=true;play(3);}
    }
    int hearts=3;
    double exposure=0,entered=0;
    Lesson previousLesson=Move;
    bool transitioning=false;
    QPolygonF lightCone()const{return {lamp,lamp+P(-420,430),lamp+P(690,430)};}
    void updateExposure(double dt){
        if(chapter&&calibration){
            exposure=std::min(1.,exposure+dt*.43);
            if(exposure>=1){loseHeart();exposure=0;damageAt=time;calibration=false;}
            return;
        }
        bool lit=chapter?litAt(hero):lesson==Lamp&&exposureArmed&&lightCone().containsPoint(hero,Qt::OddEvenFill)&&!shadowed(hero);
        exposure=std::clamp(exposure+dt*(lit?.35:-.6),0.,1.);
        if(exposure>=1){
            exposure=0;
            if(loseHeart()==0){if(chapter){if(sector>=0)beginSector(sector,true);else startChapter(true);return;}hearts=3;hero={490,210};dashLeft=0;dragging=false;draggedProjector=-1;unsetCursor();}
        }
    }
    void reset(){
        projectors.clear();draggedProjector=-1;heard.clear();rays.clear();sector=-1;relays.clear();stacks.clear();scanned=false;facesLeft=false;waveStart=-20;lampHandled=exposureArmed=false;chapter=finished=false;camera=0;hearts=3;exposure=0;entered=time;transitioning=false;lesson=Move;hero={145,340};pointer=marker;lamp={535,159};
        held.clear();hurtAt=-20;retiredAt=-20;cracks.clear();flashes.clear();dragging=false;
        dashLeft=cooldown=flash=settle=hit=walkPhase=hintTime=0; bufferedCut=false;
    }
    void tell(QString s){hint=s;hintTime=2.1;}
    void advance(Lesson next){previousLesson=lesson;transitioning=true;entered=time;lesson=next;hintTime=0;play(3);}
    bool shadowed(P p)const{
        QLineF ray(lamp,p);
        for(QLineF edge:{QLineF(column.topLeft(),column.topRight()),QLineF(column.topRight(),column.bottomRight()),
                         QLineF(column.bottomRight(),column.bottomLeft()),QLineF(column.bottomLeft(),column.topLeft())})
            if(ray.intersects(edge,nullptr)==QLineF::BoundedIntersection)return true;
        return false;
    }
    static constexpr double gateX=760;
    bool passageOpen()const{return shadowed({gateX-9,340})&&shadowed({gateX+9,340});}
    void moveBy(P change){
        auto valid=[&](P p){
            if(chapter)return (!calibration||p.x()<960)&&chapterFloor(p);
            if(!upperBounds.adjusted(7,7,-7,-7).contains(p))return false;
            if(!chapter && p.x()>520){
                if(p.y()<212||p.y()>414||((lesson!=Echo||!scanned)&&p.x()>780))return false;
                for(auto wall:baffles)if(wall.adjusted(-7,-7,7,7).contains(p))return false;
            }
            if(lesson==Lamp && std::abs(p.x()-gateX)<12 &&
               (std::abs(p.y()-340)>19 || !passageOpen()))return false;
            return field().adjusted(13,19,-13,-13).contains(p)&&
            (lesson<Lamp||!column.adjusted(-8,-8,8,8).contains(p));};
        int steps=1+int(length(change)/4);change/=steps;
        for(int n=0;n<steps;++n){P x=hero+P(change.x(),0);if(valid(x))hero=x;
            P y=hero+P(0,change.y());if(valid(y))hero=y;}
    }
    bool bufferedCut=false;
    static bool hitsGuard(P foot,QLineF blade){
        // Sample the full upright silhouette, retaining the old tolerance at the feet.
        for(int height:{0,8,16,24,32})
            if(segmentDistance(foot-P(0,height),blade)<(height==0?25:18))return true;
        return false;
    }
    void startDash(P goal){
        if(lesson<Dash||(lesson==Complete&&!chapter)||finished||dashLeft>0||cooldown>0||length(goal-hero)<4)return;
        bufferedCut=false;velocity=normalized(goal-hero);slashStart=hero;dashLeft=.18;cooldown=.50;if(std::abs(velocity.x())>.15)facesLeft=velocity.x()<0;play(1);
    }
    void finishDash(){
        QLineF crack(slashStart,hero);
        if(crack.length()<3)return;
        if(cracks.size()==3){retiringCrack=cracks.takeFirst();retiredAt=time;}
        cracks.push_back(crack);
        if(lesson==Dash){
            if(hitsGuard(dummy,crack))advance(Sheathe);
            else tell("裂痕偏离了守卫。靠近一点，瞄准守卫再冲刺。");
        }
        if(bufferedCut){bufferedCut=false;cut();}
    }
    struct Spark {
        P velocity;
        double delay, lifetime, width;
    };
    QVector<Spark> sparks;
    P impactAxis;
    static constexpr double burstDuration=.78;
    void ignite(QLineF slash){
        sparks.clear();impactAxis=normalized(slash.p2()-slash.p1());hit=burstDuration;
        auto *rng=QRandomGenerator::global();
        double heading=std::atan2(impactAxis.y(),impactAxis.x());
        for(int i=0;i<57;++i){
            double angle=heading+(i%3==0?3.14159:0)+(rng->generateDouble()-.5)*(i%5==0?5.3:1.35);
            double speed=65+310*rng->generateDouble();
            sparks.push_back({P(std::cos(angle),std::sin(angle))*speed,
                i%4==0?.065:0,.23+.46*rng->generateDouble(),.9+1.7*rng->generateDouble()});
        }
    }
    void paintImpact(QPainter&p){
        if(hit<=0)return;
        double age=burstDuration-hit;
        P origin=impactPoint+P(0,-6);
        p.save();p.setCompositionMode(QPainter::CompositionMode_Plus);
        if(age<.18){
            double strength=1-age/.18;
            QRadialGradient flare(origin,34);flare.setColorAt(0,QColor(212,218,255,int(235*strength)));
            flare.setColorAt(.4,QColor(185,87,255,int(122*strength)));flare.setColorAt(1,Qt::transparent);
            p.setBrush(flare);p.setPen(Qt::NoPen);p.drawEllipse(origin,34,34);
            P normal(-impactAxis.y(),impactAxis.x());
            p.setBrush(QColor(255,248,212,int(245*strength)));
            p.drawPolygon({origin-impactAxis*46*strength,origin-normal*5,origin+impactAxis*60*strength,origin+normal*5});
        }
        for(const auto &spark:sparks){
            double t=age-spark.delay;if(t<0||t>=spark.lifetime)continue;
            auto point=[&](double at){return origin+spark.velocity*((1-std::exp(-2.1*at))/2.1)+P(0,92*at*at);};
            double heat=1-t/spark.lifetime;
            P tip=point(t),tail=point(std::max(0.,t-.041));
            static const QColor palette[]={QColor("#54caff"),QColor("#ff64ce"),QColor("#b778ff")};QColor c=palette[(&spark-sparks.constData())%3];c.setAlpha(int(65*heat));line(p,tail,tip,c,spark.width+2.8);c.setAlpha(int(250*std::sqrt(heat)));
            line(p,tail,tip,c,spark.width);
        }
        p.restore();
    }
    void cut(){
        if(dashLeft>0){bufferedCut=true;return;}
        if(lesson<Sheathe||(lesson==Complete&&!chapter)||finished||dashLeft>0||cracks.isEmpty())return;
        flashes=cracks;cracks.clear();flash=.36;play(2);
        if(chapter){
            if(sector>=0&&!passageFound&&time>=resonanceAt){
                for(const auto &blade:flashes)if(segmentDistance(resonance,blade)<22){
                    passageFound=true;openedAt=time;stacks.removeOne(shutter);
                    impactPoint=resonance;ignite(blade);play(3);break;
                }
            }
            if(sector==4)cutWarden();
            for(auto &node:relays)if(!node.cut&&node.released>=0)
                for(auto blade:flashes)if(segmentDistance(node.pos,blade)<21){
                    node.cut=true;impactPoint=node.pos;ignite(blade);break;
                }
            for(auto &g:guards)if(g.state!=4)for(auto r:flashes)if(hitsGuard(g.pos,r)){
                if(g.armored||(g.shadowbound&&!litAt(g.pos))){g.deflected=time;break;}
                g.state=--g.hp==0?4:3;g.timer=.85;
                impactPoint=g.pos;ignite(r);break;
            }
        }
        for(auto r:flashes)if(lesson==Sheathe&&hitsGuard(dummy,r)){impactPoint=dummy;ignite(r);advance(Echo);break;}
    }
    void tick(double dt){
        if(page!=Playing){menuClock+=dt;return;}
        time+=dt;cooldown=std::max(0.,cooldown-dt);flash=std::max(0.,flash-dt);
        hit=std::max(0.,hit-dt);hintTime-=dt;
        if(finished)return;
        updateExposure(dt);
        if(lesson==Complete&&!chapter)return;
        if(dashLeft>0){
            double part=std::min(dt,dashLeft);moveBy(velocity*(905*part));dashLeft-=part;
            if(dashLeft<=0)finishDash();
        }else{
            P d(int(held.contains(Qt::Key_D)||held.contains(Qt::Key_Right))-int(held.contains(Qt::Key_A)||held.contains(Qt::Key_Left)),
                int(held.contains(Qt::Key_S)||held.contains(Qt::Key_Down))-int(held.contains(Qt::Key_W)||held.contains(Qt::Key_Up)));
            if(length(d)>0){if(d.x()!=0)facesLeft=d.x()<0;P last=hero;moveBy(normalized(d)*dt*174);walkPhase+=length(hero-last)*.069;}
        }
        if(chapter){runChapter(dt);return;}
        if(lesson==Move&&length(hero-marker)<23)advance(Dash);
        if(lesson==Echo&&scanned&&hero.x()>796)startChapter(false);
        if(lesson==Lamp){
            if(lampHandled && length(hero-safeSpot)>5)exposureArmed=true;

            if(lampHandled&&passageOpen()&&hero.x()>gateX+20){startChapter(false);}
        }
    }
    static void line(QPainter&p,P a,P b,QColor c,double w=1){
        p.setPen(QPen(c,w,Qt::SolidLine,Qt::RoundCap));p.drawLine(a,b);
    }
    static void text(QPainter&p,QRectF r,QString str,int size,QColor c,bool bold=false,int align=Qt::AlignLeft|Qt::AlignVCenter){
        QFont font("Microsoft YaHei");font.setPixelSize(size);font.setBold(bold);
        p.setFont(font);p.setPen(c);p.drawText(r,align,str);
    }
    static void ring(QPainter&p,P c,double r,QColor color,double w=1){
        p.setPen(QPen(color,w));p.setBrush(Qt::NoBrush);p.drawEllipse(c,r,r);
    }
    static void shape(QPainter&p,QPolygonF poly,QColor fill,QColor edge){
        p.setBrush(fill);p.setPen(QPen(edge,1.3));p.drawPolygon(poly);
    }
    void lights(QPainter&p){
        QPainterPath obstruction;
        for(QLineF edge:{QLineF(column.topLeft(),column.topRight()),QLineF(column.topRight(),column.bottomRight()),
                         QLineF(column.bottomRight(),column.bottomLeft()),QLineF(column.bottomLeft(),column.topLeft())}){
            QPainterPath part;part.addPolygon({edge.p1(),edge.p2(),lamp+(edge.p2()-lamp)*12,lamp+(edge.p1()-lamp)*12});obstruction=obstruction.united(part);}
        QPainterPath cone;cone.addPolygon(lightCone());
        QRadialGradient glow(lamp,500);glow.setColorAt(0,QColor(248,200,120,77));glow.setColorAt(1,QColor(229,180,113,12));p.fillPath(cone.subtracted(obstruction),glow);
        p.setPen(QPen(QColor("#807361"),1));p.setBrush(QColor("#252523"));p.drawRect(column);
        for(int x=705;x<747;x+=9)line(p,{double(x),207},{double(x)-2,232},QColor("#524c40"));
        line(p,{506,159},{872,159},QColor("#6b5942"));ring(p,lamp,17,amber,2);
        p.setPen(Qt::NoPen);p.setBrush(bone);p.drawEllipse(lamp,6,6);
        const QColor steel("#56625f");
        for(double offset:{-4.,4.}){
            line(p,{gateX+offset,field().top()},{gateX+offset,309},steel,2);
            line(p,{gateX+offset,371},{gateX+offset,field().bottom()},steel,2);
        }
        QColor edge=passageOpen()?mint:amber;
        line(p,{gateX-9,309},{gateX+9,309},edge,3);
        line(p,{gateX-9,371},{gateX+9,371},edge,3);
        if(!passageOpen())for(int y=314;y<370;y+=8)
            line(p,{gateX-3,double(y)},{gateX+3,double(y+4)},edge,2);
        else{line(p,{gateX+14,333},{gateX+22,340},mint,2);line(p,{gateX+22,340},{gateX+14,347},mint,2);}
    }
    void fracture(QPainter&p,QLineF l,bool active){
        if(l.length()<1)return;
        P n=normalized(P(-l.dy(),l.dx()))*(active?3.2:1.6);
        p.setPen(Qt::NoPen);
        auto blade=[&](P offset,P width,QColor color){
            p.setBrush(color);p.drawPolygon({l.p1()+offset,l.pointAt(.16)+width+offset,
                l.pointAt(.84)+width+offset,l.p2()+offset,l.pointAt(.84)-width+offset,l.pointAt(.16)-width+offset});
        };
        blade({},n*.58,Qt::white);
    }
#include "actors.inc"
    void controlGlyph(QPainter &p,P at,Lesson step){
        p.save();p.translate(at);p.setPen(QPen(bone,1.4));p.setBrush(QColor("#151723"));
        if(step==Move||step==Echo){
            QStringList keys=step==Move?QStringList{"W","A","S","D"}:QStringList{"SPACE"};
            double w=step==Move?22:61,span=keys.size()*(w+4)-4;
            for(int i=0;i<keys.size();++i){QRectF box(-span/2+i*(w+4),-11,w,22);
                p.drawRoundedRect(box,3,3);text(p,box,keys[i],12,bone,true,Qt::AlignCenter);}
        }else{
            QRectF shell(-10,-14,20,28);p.drawRoundedRect(shell,8,8);
            QPainterPath body;body.addRoundedRect(shell,8,8);p.setClipPath(body,Qt::IntersectClip);
            p.fillRect(QRectF(step==Sheathe?1:-10,-14,9,13),step==Sheathe?QColor("#fa93cc"):QColor("#85d9ff"));
            line(p,{-10,0},{10,0},bone);
        }
        p.restore();
    }
    void lessonText(QPainter &p,Lesson step,double opacity){
        if(opacity<=0)return;
        static const QStringList words={"WASD 移动，走入圆环","瞄准守卫，左键冲刺穿过它","右键收刀，引爆裂痕","空格探测隔板，绕过三道障碍向右走","光下曝光上升。拖灯，让柱影盖住右侧闸口","训练完成"};
        static const P anchors[]={{300,293},{448,284},{448,284},{626,174},{706,113},{746,385}};
        const P at=anchors[step];p.save();p.setOpacity(std::clamp(opacity,0.,1.));
        if(step!=Complete)controlGlyph(p,at-P(0,30),step);
        text(p,{at.x()-260,at.y()-18,520,36},step==lesson&&hintTime>0?hint:step==Lamp&&passageOpen()?QStringLiteral("影中曝光回落。穿过右侧闸口"):words[step],
             17,step==Complete?mint:bone,false,Qt::AlignCenter);p.restore();
    }
    void drawVitals(QPainter &p){
        QRectF view=field();p.save();p.translate(view.left()+30,view.bottom()-53);
        p.setRenderHint(QPainter::Antialiasing,false);
        static const char *heart[]={"0110110","1111111","1111111","0111110","0011100","0001000"};
        for(int i=0;i<3;++i)for(int y=0;y<6;++y)for(int x=0;x<7;++x){
            if(heart[y][x]=='0')continue;
            QColor color=i<hearts?(y==1&&x==1?QColor("#ffd2c4"):QColor("#ef827e")):QColor("#4a3438");
            p.fillRect(QRectF(i*27+x*3,-14+y*3,3,3),color);
        }
        text(p,{0,10,36,22},"曝光",12,muted);
        for(int i=0;i<16;++i)p.fillRect(QRectF(38+6*i,17,5,7),
            i<int(std::ceil(exposure*16))?amber:QColor("#303632"));
        p.restore();
    }
    void echoMap(QPainter &p){
        QRectF panel(field().left()+14,field().top()+12,247,115);p.save();
        p.setPen(QPen(QColor("#30414e"),1));p.setBrush(QColor(5,9,16,215));p.drawRect(panel);
        const auto bounds=chapter?chapterBounds():upperBounds;
        auto map=[&](P q){return panel.topLeft()+P(4,4)+(q-bounds.topLeft())*std::min(239./bounds.width(),107./bounds.height());};
        // Batch short ray contacts by age to keep repeated scans inexpensive.
        QVector<QLineF> bands[5];
        for(const auto &strip:heard){
            double age=time-strip.arrives;if(age<0||age>=15)continue;
            bands[std::min(4,int(age/3))].append(QLineF(map(strip.edge.p1()),map(strip.edge.p2())));
        }
        for(int i=0;i<5;++i){p.setPen(QPen(QColor(123,222,227,235-40*i),1.4));p.drawLines(bands[i]);}
        p.setClipRect(panel.adjusted(1,1,-1,-1));
        auto mark=[&](P at,QColor color,bool lampIcon){
            P c=map(at);
            if(lampIcon){shape(p,{c-P(0,4),c+P(4,0),c+P(0,4),c-P(4,0)},color,color);}
            else{shape(p,{c-P(0,3),c+P(3,3),c+P(-3,3)},color,color);}
        };
        if(chapter){
            if(sector!=2)mark(stageLamp,amber,true);
            for(const auto &g:projectors)mark(g.source,g.color,true);
            for(const auto &g:guards)if(g.state!=4){
                if((sector<0||hero.x()>935)&&length(hero-g.pos)<190&&clearGuardLine(hero,g.pos))
                    mark(g.pos,QColor("#ff779c"),false);
                else if(time>=g.mapAt&&time-g.mapAt<15)mark(g.mapContact,QColor("#b86791"),false);
            }
            if(sector==4&&boss.active&&boss.hp)mark(boss.core,QColor("#ff779c"),false);
        }
        if(!chapter&&lesson==Lamp)mark(lamp,amber,true);
        p.setBrush(bone);p.setPen(Qt::NoPen);p.drawEllipse(map(hero),2.7,2.7);p.restore();
    }
    void drawChapter(QPainter&p){
        QRectF well(1148,98,134,420);
        if(sector<0)p.fillRect(well,QColor("#050b0d"));
        if(sector<0)for(int y=104;y<514;y+=11)line(p,{1151.,double(y)},{1161.,double(y+5)},QColor("#345051"));
        for(auto c:channels())for(double x=c.pit.left();x<=c.pit.right();x+=7){
            if(!bridgeShade({x,c.crossing}))continue;
            line(p,{x,c.crossing-25},{x,c.crossing+25},QColor("#56857d"),2);
            line(p,{x,c.crossing-20},{x+5,c.crossing-15},QColor("#314842"));
        }
        if(sector!=2){
        p.save();p.setClipRect(chapterBounds(),Qt::IntersectClip);
        QPainterPath lit=projectedLight(),shade;const P origin=lensPosition();
        for(QLineF e:{QLineF(pier.topLeft(),pier.topRight()),QLineF(pier.bottomLeft(),pier.bottomRight()),QLineF(pier.topLeft(),pier.bottomLeft()),QLineF(pier.topRight(),pier.bottomRight())}){
            QPainterPath q;q.addPolygon({e.p1(),e.p2(),origin+(e.p2()-origin)*40,origin+(e.p1()-origin)*40});shade=shade.united(q);}
        QRadialGradient glow(origin,1060);glow.setColorAt(0,QColor(255,201,119,sector<0?76:36));glow.setColorAt(1,QColor(223,161,81,sector<0?15:6));if(!calibration)p.fillPath(lit.subtracted(shade),glow);p.restore();
        if(sector<0||sector==4){p.fillRect(pier,QColor("#655e4e"));line(p,pier.topLeft(),pier.topRight(),amber,2);}
        line(p,{sector==4?1130.:799.,stageLamp.y()-3},{sector==4?1640.:1400.,stageLamp.y()-3},QColor("#263d53"),2);
        line(p,{sector==4?1130.:799.,stageLamp.y()+3},{sector==4?1640.:1400.,stageLamp.y()+3},QColor("#4d304c"),2);
        if(calibration){
            QPainterPath beam;beam.addPolygon({stageLamp,hero+P(-25,8),hero+P(25,8)});
            QLinearGradient gradient(stageLamp,hero);gradient.setColorAt(0,QColor(255,202,135,25));
            gradient.setColorAt(1,QColor(255,202,135,87));p.fillPath(beam,gradient);
        }
        p.save();p.translate(stageLamp);
        line(p,{-17,-7},{17,-7},QColor("#81889e"),4);
        p.rotate(calibration?std::atan2(hero.y()-stageLamp.y(),hero.x()-stageLamp.x())*57.29578-90:
                              std::atan2(beamDirection().y(),beamDirection().x())*57.29578-90);
        shape(p,{{-12,-12},{8,-14},{15,-6},{12,10},{-12,10},{-16,-3}},QColor("#23263c"),QColor("#737e98"));
        line(p,{-12,-6},{-12,3},QColor("#65d8ff"),3);line(p,{11,-7},{10,2},QColor("#ff8bd4"),3);
        shape(p,{{-11,7},{11,7},{15,13},{-15,13}},QColor("#4d4651"),QColor("#9d94a8"));
        QRadialGradient lens(P(0,14),22);lens.setColorAt(0,sector==4&&lensBlocked()?QColor(198,70,160,110):QColor(255,220,151,175));lens.setColorAt(1,Qt::transparent);
        p.setBrush(lens);p.setPen(Qt::NoPen);p.drawEllipse(P(0,14),22,13);line(p,{-10,13},{10,13},QColor("#ffe6b5"),3);p.restore();
        }
        if(medkit&&(sector<0||(hearts<3&&length(hero-kitPosition)<110))){
            P c=kitPosition+P(0,2*std::sin(time*3));ring(p,kitPosition,15,QColor("#764362"));
            p.setPen(Qt::NoPen);p.setBrush(QColor("#ffa1cf"));p.drawRect(QRectF(c.x()-3,c.y()-9,6,18));
            p.drawRect(QRectF(c.x()-9,c.y()-3,18,6));
            if(hero.x()>1220)text(p,{c.x()-65,c.y()+20,130,24},"拾取回复一颗心",13,mint,false,Qt::AlignCenter);
        }else if(time-healedAt<1.4)text(p,{kitPosition.x()-48,298,96,24},"生命 +1",15,mint,false,Qt::AlignCenter);
        if(sector==3&&wave==0&&reinforcementsAt>=0){
            double left=std::max(0.,reinforcementsAt-time);
            for(P at:eastWing){
                ring(p,at,11,amber,2);ring(p,at,13+left*19,QColor("#b95785"));
                line(p,at-P(5,0),at+P(5,0),bone);
            }
        }
        for(auto &g:guards)if(g.state!=4){
            if(sector>=0&&hero.x()<935)continue;
            if(sector>=0&&!clearGuardLine(hero,g.pos)){
                double age=time-g.lastAudible;
                if(age<1.4)ring(p,g.pos,9+age*7,QColor(187,148,216,int(165*(1-age/1.4))));
                continue;
            }
            if(g.fast){P a=g.pos-P(0,30);line(p,a-P(7,2),a+P(4,-6),QColor("#89ecff"),3);}
            if(g.state==1)line(p,g.pos,g.pos+g.attack*94,QColor(251,103,70,138),5);
            p.save();p.translate(g.pos-dummy);drawEnemy(p,&g);p.restore();
            if(sector>=0){
                P c=g.pos-P(0,18);
                if(g.armored){
                    const QColor rim=time-g.deflected<.23?bone:QColor("#80cfff");
                    QPolygonF shell{c+P(-15,-16),c+P(0,-22),c+P(15,-16),c+P(19,4),c+P(0,18),c+P(-19,4)};
                    p.setPen(QPen(rim,1.6));p.setBrush(QColor(84,173,255,30));p.drawPolygon(shell);
                    p.setBrush(Qt::NoBrush);p.setPen(QPen(amber,3));p.drawArc(QRectF(c-P(23,23),QSizeF(46,46)),90*16,-int(5760*g.heat/1.25));
                }
                if(g.shadowbound){
                    QColor edge=litAt(g.pos)?amber:QColor("#b28aff");
                    for(int j=0;j<4;++j){
                        double a=time*2+j*1.5708;P d(std::cos(a)*16,std::sin(a)*20);
                        line(p,c+d,c+d*.67,edge,3);
                    }
                }
            }}
        if(sector>=0&&passageFound&&time-openedAt<1.4){
            double v=std::clamp((time-openedAt)/1.4,0.,1.);
            p.save();p.setOpacity(1-v);ring(p,resonance,12+43*v,mint,2);
            ring(p,shutter.center(),6+56*v,mint,1.7);p.restore();
        }
        if(sector>=0&&length(hero-resonance)<155){
            QColor c=passageFound?mint:time>=resonanceAt?amber:QColor("#847aba");
            for(int j=0;j<3;++j)line(p,resonance+P(-8+j*8,-4-j%2*4),resonance+P(-8+j*8,4+j%2*4),c,2);
            if(!passageFound)ring(p,resonance,12+std::sin(time*3)*2,c);
        }
        if(sector<0||(length(hero-exitPoint)<180&&clearGuardLine(hero,exitPoint))){
            p.save();p.translate(exitPoint);p.setPen(QPen(mint,2));p.setBrush(QColor("#102323"));
            p.drawRect(QRectF(-13,-31,26,37));
            if(doorOpen()){p.fillRect(QRectF(-10,-29,20,33),QColor("#04090c"));line(p,{-11,4},{10,4},mint,2);}
            else{p.fillRect(QRectF(-10,-29,20,33),QColor("#203037"));line(p,{0,-27},{0,4},muted,1);}
            p.fillRect(QRectF(-12,-47,24,13),QColor("#2c7468"));
            p.setPen(Qt::NoPen);p.setBrush(bone);p.drawEllipse(P(3,-45),2,2);
            line(p,{2,-42},{-2,-38},bone,2);line(p,{-2,-38},{-8,-35},bone,1.6);
            line(p,{-2,-38},{3,-35},bone,1.6);line(p,{1,-42},{6,-40},bone,1.6);
            line(p,{-7,-43},{-2,-43},bone,1.3);p.restore();
        }
        if(sector==2)drawProjectors(p);
        if(sector==4)paintWarden(p);
        
        for(const Relay&r:relays)if(r.seen&&!r.cut&&!calibration){
            const bool active=r.released>=0;
            const double age=time-r.released;
            QColor core=active?QColor("#ff90df"):amber;
            ring(p,r.pos,12,core,2);
            if(active){
                shape(p,{r.pos+P(0,-9),r.pos+P(7,0),r.pos+P(0,9),r.pos-P(7,0)},core,bone);
                ring(p,r.pos,16+2*std::sin(time*6),core);
            }else{p.setPen(QPen(bone,3));p.setBrush(Qt::NoBrush);p.drawArc(QRectF(r.pos-P(18,18),QSizeF(36,36)),90*16,-int(5760*r.charge/1.35));}
            if(!active||age<.72)for(int j=0;j<4;++j){
                P d(std::cos(j*1.5708),std::sin(j*1.5708)),side(-d.y(),d.x());
                P c=r.pos+d*(17+(active?age*73:0));
                line(p,c-side*6,c+side*6,QColor(197,157,220,active?int(250*(1-age/.72)):235),3);
            }
        }
        if(sector>=0){

            text(p,{camera+110,cameraY()+20,780,38},finished?"第一关完成":QString("%1F   /   %2").arg(sector+1).arg(rooms[sector]),22,bone,true,Qt::AlignCenter);
            QString clue;
            if(finished)clue="Esc 返回选关";
            else if(sealed()){const Relay *r=nullptr;for(auto &node:relays)if(!node.cut){r=&node;break;}
                clue=!r->seen&&sector==1?"东南耳室只收反射波。靠近南墙发声，可绕开哨兵。":!r->seen?(r->pos.x()<1200?"西南角有回响。靠近后发出声波。":"东南侧有回响。靠近后扫描。"):
                    r->released>=0?"冲刺穿过核心，右键收刀击破。":powered(*r)?"保持照射，直到锁环松开。":"拖动光源，让光直接照在装置上。";}
            else clue.clear();
            if(sector==1&&!guards.isEmpty()&&guards.front().armored&&length(hero-guards.front().pos)<240)clue="让光持续照住蓝盾，再对守卫收刀。";
            
            if(sector==2&&!finished){
                const auto &seal=relays.first();
                clue=seal.cut?QString():seal.released>=0?"封口已松。穿过中央菱形，收刀。":
                    !seal.seen?"圆盘下传来回响。靠近后按空格。":
                    "沿顶端滑轨拖灯，让两道柱影分别盖住同色刻点。";
            }
            if(sector==3&&!finished)clue=wave==0?(cleared()?"继续向右，进入中央环廊。":"青羽守卫冲得更快。侧移，等它冲进裂痕。"):
                cleared()?QString():"紫影守卫只在光里受伤。";
            if(sector==3&&!finished&&wave==0&&reinforcementsAt>=0)clue="增援即将抵达，准备迎战。";
            if(sector==4&&!finished)clue=!boss.hp?QString():boss.open>0?"核心裸露！冲刺穿过青色核心，右键收刀。":!boss.globs.isEmpty()?"冲刺穿过黏球，再收刀清理灯口。":"按住灯向场内瞄准。光束截住激光，持续照射可击破护壳。";
            if(!passageFound&&length(hero-resonance)<150)clue=time>=resonanceAt?"共鸣点已显形。冲刺穿过它，再收刀。":"这里有一道近处的回声。";
            text(p,{camera+80,cameraY()+63,840,28},clue,15,muted,false,Qt::AlignCenter);return;
        }
        if(time-arrival<4||finished){
            p.save();p.setOpacity(finished?1.:std::clamp(4-(time-arrival),0.,1.));
            text(p,{camera+290,47,420,42},finished?"训练完成":"下层训练   /   光与影",23,bone,true,Qt::AlignCenter);p.restore();}
        if(!calibration&&time-damageAt>1.5&&sealed()){
            const Relay&r=relays.front();
            bool linked=false;for(auto l:cracks)if(segmentDistance(r.pos,l)<21)linked=true;
            controlGlyph(p,r.pos+P(0,41),r.released<0?Lamp:linked?Sheathe:Dash);
            text(p,{r.pos.x()-235,r.pos.y()+57,470,28},r.released<0?"向左拖动光源，让斜射光束照亮装置":linked?"右键收刀，击破核心":"左键冲刺，穿过裸露的核心",16,bone,false,Qt::AlignCenter);
            return;
        }
        if(!calibration&&time-damageAt>1.5&&hero.x()<1148)controlGlyph(p,{1095,362},Lamp);
        if(calibration||time-damageAt<1.5)text(p,{hero.x()-205,hero.y()-73,410,30},calibration?"光源正在追踪你 · 留意左下角曝光条":"曝光已满 · 失去一颗心",17,amber,false,Qt::AlignCenter);
        else if(hero.x()<1148)text(p,{845,374,510,25},"按住金色光源左右拖动，让柱影连成桥，再走到对岸",14,muted,false,Qt::AlignCenter);
        else if(hero.x()>1282)text(p,{1545,306,250,25},QString(),14,muted,false,Qt::AlignCenter);
    }
    const quint32 floorSeed=QRandomGenerator::global()->generate();
    quint32 floorHash(int x,int y)const{
        quint32 h=floorSeed^(quint32(x)*0x9e3779b9u)^(quint32(y)*0x85ebca6bu);
        h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;
    }
    bool pinkTile(int x,int y)const{
        // Each pair of cells shares its nearest jittered seed. Manhattan
        // distance makes the boundaries step along the square grid.
        int col=int(std::floor(x/2.)),sx=int(std::floor(col/2.)),sy=int(std::floor(y/3.));
        int best=100;bool pink=false;
        for(int j=sy-1;j<=sy+1;++j)for(int i=sx-1;i<=sx+1;++i){
            quint32 h=floorHash(i,j);
            int d=std::abs(col-(2*i+int(h%2)))+std::abs(y-(3*j+int((h>>3)%3)));
            if(d<best){best=d;pink=(h&1024)!=0;}
        }
        return pink;
    }
    enum Page {Title,Select,Playing};
    Page page=Title;
    double menuClock=0;
    P hover{-100,-100};
    QRectF chooseButton{375,415,250,50},quitButton{375,483,250,50},course{111,176,163,47},backButton{110,492,109,39};
    QRectF floorButton(int n)const{return {111.+n*157,298.+(n%2)*36,148,58};}
    void menuAction(P q){
        if(page==Title){if(quitButton.contains(q)){close();return;}if(!chooseButton.contains(q))return;page=Select;play(0);}
        else if(course.contains(q)){reset();page=Playing;play(3);}
        else if(backButton.contains(q))page=Title;
        else for(int i=0;i<rooms.size();++i)if(floorButton(i).contains(q)){
            reset();beginSector(i,true);page=Playing;play(3);break;
        }
        unsetCursor();update();
    }
    void menuArt(QPainter &p){
        const QColor cyan("#56deff"),pink("#ff5eb8");
        const double beat=std::sin(menuClock*3);
        // Layered arcade cabinets frame the stage without competing with the title.
        for(int side=0;side<2;++side){
            p.save();if(side){p.translate(1000,0);p.scale(-1,1);}
            QColor accent=side?pink:cyan;
            shape(p,{{0,0},{220,0},{179,37},{112,37},{81,72},{81,134},{44,174},{44,448},{83,488},{83,548},{147,615},{0,615}},QColor("#15101f"),QColor("#463048"));
            for(int i=0;i<5;++i){double x=12+i*7;line(p,{x,183+i*2.},{x,438-i*2.},QColor("#423145"),3);}
            for(int i=0;i<7;++i){double x=13+i*18;p.fillRect(QRectF(x,12,12,4),i<4?accent:QColor("#4c354b"));}
            line(p,{45,172},{80,134},accent,3);line(p,{44,448},{81,486},accent,3);
            for(int i=0;i<12;++i){double x=80+i*14,h=12+((i*37+side*19)%61);
                p.fillRect(QRectF(x,615-h,13,h),QColor("#1e172a"));p.fillRect(QRectF(x+3,619-h,3,4),accent.darker(240));}
            p.restore();
        }
        auto button=[&](QRectF r,QString name,QColor edge){
            bool hot=r.contains(hover);double x=r.left(),y=r.top(),w=r.width(),h=r.height();
            QPolygonF panel{{x+10,y},{x+w,y},{x+w,y+h-10},{x+w-10,y+h},{x,y+h},{x,y+10}};
            shape(p,panel.translated(0,4),edge.darker(410),edge.darker(310));
            shape(p,panel,hot?edge.darker(410):QColor("#181322"),hot?bone:edge);
            line(p,{x+13,y+4},{x+56,y+4},edge,2);
            for(int j=0;j<3;++j)line(p,{x+w-31+j*6,y+h-7},{x+w-27+j*6,y+h-13},edge,1.6);
            text(p,r,name,21,bone,true,Qt::AlignCenter);
        };
        if(page==Select){
            text(p,{108,58,520,66},"选择关卡",39,bone,true);
            // Topographic contours sit behind the route, with a quiet centre for legibility.
            for(int row=0;row<17;++row){
                QPainterPath contour;
                for(int x=93;x<=908;x+=5){
                    double y=191+row*13+23*std::sin(x*.013+row*.13)+15*std::sin(x*.031-row*.09);
                    if(x==93)contour.moveTo(x,y);else contour.lineTo(x,y);
                }
                p.setPen(QPen(QColor(85,115,149,40),1));p.setBrush(Qt::NoBrush);p.drawPath(contour);
            }
            button(course,"新手训练",cyan);
            for(int i=0;i<4;++i)line(p,floorButton(i).center(),floorButton(i+1).center(),QColor("#566076"),2);
            for(int i=0;i<rooms.size();++i){
                auto r=floorButton(i);button(r,rooms[i],i==4?pink:cyan);
                text(p,{r.left(),r.top()-32,r.width(),26},QString("0%1 F").arg(i+1),15,muted,true,Qt::AlignCenter);
            }
            button(backButton,"返回",pink);return;
        }
        // Three interrupted echo arcs terminate at the cut through the title.
        p.setBrush(Qt::NoBrush);
        for(int i=0;i<3;++i){int r=47+10*i;p.setPen(QPen(i==1?pink.darker(155):cyan.darker(155),i==1?2:1));
            p.drawArc(QRectF(620-r,161-r,2*r,2*r),-65*16,195*16);}
        QFont font("Microsoft YaHei");font.setPixelSize(page==Title?107:64);font.setWeight(QFont::Black);
        QPainterPath raw;raw.addText(0,0,font,page==Title?"光与影":"选择关卡");
        QTransform shear;shear.shear(-.13,0);QPainterPath mark=shear.map(raw);
        mark.translate(500-mark.boundingRect().center().x(),(page==Title?169:191)-mark.boundingRect().center().y());
        for(int i=0;i<38;++i){double a=i*2.39996;P d(std::cos(a),std::sin(a));
            P tip(500+d.x()*(217+i%5*9),165+d.y()*(70+i%6*5));P base(500+d.x()*150,165+d.y()*40);
            QPolygonF shard{base,tip,base+P(-d.y(),d.x())*(4+i%8)};
            shape(p,shard,i%2?QColor("#381432"):QColor("#102a41"),i%3?pink.darker(190):cyan.darker(160));}
        p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(230,55,166,31),11));p.drawPath(mark);
        p.fillPath(mark.translated(-4,-1),cyan);p.fillPath(mark.translated(4,4),pink);
        QLinearGradient enamel(0,112,0,225);enamel.setColorAt(0,Qt::white);enamel.setColorAt(.54,QColor("#fff1ff"));enamel.setColorAt(1,QColor("#a998cf"));p.fillPath(mark,enamel);
        p.save();p.setClipPath(mark);
        for(int i=0;i<12;++i)line(p,{315.+i*36,220},{364.+i*36,115},QColor(74,65,115,84),2);
        shape(p,{{308,185},{465,162},{484,167},{653,129},{487,174},{462,169}},QColor("#1a152b"),pink);
        line(p,{361,148},{403,141},QColor("#242138"),6);line(p,{364,147},{398,142},cyan,2);p.restore();
        shape(p,{{607,139},{678,110},{650,134},{632,137}},bone,cyan);
        for(int k=0;k<5;++k){double x=329-k*8;line(p,{x,188+k*2.},{x+5,182+k*2.},k%2?cyan:pink,2);}
        if(page==Title){
            QRadialGradient pool(P(500,348),85);pool.setColorAt(0,QColor(109,60,173,108));pool.setColorAt(1,Qt::transparent);
            p.setPen(Qt::NoPen);p.setBrush(pool);p.drawEllipse(P(500,348),86,17);
            p.save();p.translate(500,321+beat*2);p.scale(2.55,2.55*(1+.024*beat));p.translate(-hero);drawHero(p);p.restore();
            for(int j=0;j<14;++j){double f=std::fmod(menuClock*.28+j*.173,1.);p.fillRect(QRectF(428+(j*47)%145,367-f*88,2,2),j%2?pink.darker(175):cyan.darker(185));}
            button(chooseButton,"开始游戏",cyan);button(quitButton,"退出游戏",pink);
        }
    }
    void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),Qt::black);
        double s=std::min(width()/1000.,height()/615.);p.translate((width()-1000*s)/2,(height()-615*s)/2);p.scale(s,s);
        QRectF area=field().translated(camera,cameraY());p.save();p.translate(-camera,-cameraY());
        double hurtAge=time-hurtAt;
        if(page==Playing&&hurtAge>=0&&hurtAge<.24){
            double amplitude=4.5*(1-hurtAge/.24);
            p.translate(amplitude*std::cos(91*hurtAge),amplitude*std::sin(73*hurtAge));
        }
        constexpr int cell=28;
        for(int y=int(std::floor(area.top()/cell));y*cell<area.bottom();++y){
            for(int x=int(std::floor(area.left()/cell));x*cell<area.right();++x){
                const QColor color=pinkTile(x,y)?QColor("#280b20"):QColor("#081c30");
                const P corner(x*cell,y*cell);
                line(p,corner,corner+P(cell,0),color,.8);
                line(p,corner,corner+P(0,cell),color,.8);
            }
        }
        if(page!=Playing){p.restore();menuArt(p);return;}
        for(int i=0;i<(sector>=0?28:64);++i){
            P pt(area.left()+std::fmod(i*173.4+time*(4+i%7),area.width()),area.top()+std::fmod(i*91.3+time*(3+i%4),area.height()));
            QColor c=i%2?QColor("#be509b"):QColor("#468eb9");c.setAlpha(25+int(40*(1+std::sin(time*2+i))));
            line(p,pt,pt+P(i%6==0?10:2,-1),c,1);}
        if(!chapter&&lesson==Lamp)lights(p);
        if(chapter)drawChapter(p);
        if(!chapter&&lesson==Echo){line(p,{787,335},{796,343},mint,2);line(p,{796,343},{787,351},mint,2);}
        paintEcho(p);
        if(lesson==Move){ring(p,marker,21,mint,2);ring(p,marker,26+3*std::sin(time*3),QColor("#416056"));
        }
        if(lesson==Dash||lesson==Sheathe){
            drawEnemy(p);}
        if(time-retiredAt<.36){p.save();p.setOpacity(1-(time-retiredAt)/.36);fracture(p,retiringCrack,false);p.restore();}
        for(auto l:cracks)fracture(p,l,false);if(dashLeft>0)fracture(p,{slashStart,hero},false);
        if(flash>0){p.setOpacity(flash/.36);for(auto l:flashes)fracture(p,l,true);p.setOpacity(1);}
        paintImpact(p);
        p.save();
        if(hurtAge>=0&&hurtAge<.44&&int(hurtAge/.065)%2==0)p.setOpacity(.30);
        drawHero(p);p.restore();p.restore();
        double age=time-entered;
        if(!chapter&&transitioning&&age<.55)lessonText(p,previousLesson,1-age/.55);
        double alpha=transitioning?(age-.55)/.35:1;
        if(lesson==Complete)alpha=std::min(alpha,(3.4-age)/.9);
        if(!chapter)lessonText(p,lesson,alpha);drawVitals(p);
        if(lesson>=Echo)echoMap(p);
        if(sector==4&&boss.active&&boss.hp>0){
            p.fillRect(QRectF(326,8,348,6),QColor("#392637"));
            p.fillRect(QRectF(326,8,348.*boss.hp/Warden::maxHp,6),QColor("#f380ac"));
        }
    }
    double cameraY()const{return chapter?hero.y()-std::clamp(hero.y(),160.,455.):0.;}
    P mapPoint(P q)const{double s=std::min(width()/1000.,height()/615.);return(q-P((width()-1000*s)/2,(height()-615*s)/2))/s+P(camera,cameraY());}
    void mousePressEvent(QMouseEvent*e)override{
        if(page!=Playing){if(e->button()==Qt::LeftButton)menuAction(mapPoint(e->position())-P(camera,cameraY()));return;}
        pointer=mapPoint(e->position());
        if(finished||!field().translated(camera,cameraY()).contains(pointer))return;
        if(e->button()==Qt::LeftButton){
            for(int i=0;i<projectors.size();++i)if(length(pointer-projectors[i].source)<24){
                dragging=true;draggedProjector=i;setCursor(Qt::ClosedHandCursor);return;
            }
            if(sector!=2&&(chapter||lesson==Lamp)&&!calibration&&length(pointer-(chapter?stageLamp:lamp))<24){dragging=true;setCursor(Qt::ClosedHandCursor);}
            else startDash(pointer);
        }else if(e->button()==Qt::RightButton)cut();
    }
    void mouseMoveEvent(QMouseEvent*e)override{if(page!=Playing){hover=mapPoint(e->position())-P(camera,cameraY());setCursor((page==Title?(chooseButton.contains(hover)||quitButton.contains(hover)):(course.contains(hover)||floorButton(0).contains(hover)||floorButton(1).contains(hover)||floorButton(2).contains(hover)||floorButton(3).contains(hover)||floorButton(4).contains(hover)||backButton.contains(hover)))?Qt::PointingHandCursor:Qt::ArrowCursor);return;}pointer=mapPoint(e->position());if(dragging&&draggedProjector>=0){
            auto &g=projectors[draggedProjector];g.source.setX(std::clamp(pointer.x(),g.left,g.right));rebuildProjection(g);
        }else if(dragging&&sector==4){
            if(pointer.y()<stageLamp.y()-20)boss.lightDir=normalized(pointer-stageLamp);
            else if(boss.globs.isEmpty())stageLamp.setX(std::clamp(pointer.x(),1130.,1640.));
        }else if(dragging&&chapter)stageLamp.setX(std::clamp(pointer.x(),sector==4?1130.:799.,sector==4?1640.:1400.));
        else if(dragging){
            double next=std::clamp(pointer.x(),506.,872.);
            if(!lampHandled&&std::abs(next-lamp.x())>1){lampHandled=true;safeSpot=hero;}
            lamp.setX(next);
        }
        else if(std::any_of(projectors.begin(),projectors.end(),[&](const Projector &g){return length(pointer-g.source)<24;})||
                (sector!=2&&(chapter||lesson==Lamp)&&!calibration&&length(pointer-(chapter?stageLamp:lamp))<24))setCursor(Qt::OpenHandCursor);else unsetCursor();}
    void mouseReleaseEvent(QMouseEvent*e)override{if(e->button()==Qt::LeftButton){dragging=false;draggedProjector=-1;unsetCursor();}}
    void keyPressEvent(QKeyEvent*e)override{if(e->isAutoRepeat())return;if(page!=Playing){if(e->key()==Qt::Key_Return)menuAction(page==Title?chooseButton.center():course.center());else if(e->key()==Qt::Key_Escape){if(page==Title)close();else page=Title;}return;}if(e->key()==Qt::Key_Space){pulse();return;}if(e->key()==Qt::Key_Escape){reset();page=Select;return;}if(e->key()==Qt::Key_R){if(sector>=0)beginSector(sector,true);else if(chapter)startChapter(true);else reset();unsetCursor();}else held.insert(e->key());}
    void keyReleaseEvent(QKeyEvent*e)override{if(!e->isAutoRepeat())held.remove(e->key());}
    void focusOutEvent(QFocusEvent*)override{held.clear();dragging=false;draggedProjector=-1;unsetCursor();}
public:
    Training(){resize(1000,615);setMinimumSize(750,462);setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
        setWindowTitle("光与影");prepareTones();elapsed.start();
        connect(&timer,&QTimer::timeout,this,[this]{tick(std::min(.04,elapsed.restart()/1000.));update();});timer.start(16);}
    // Flood-fill the actual collision map. Lamp changes may only be made from dry land.
    bool verifySectorNavigation(){
        constexpr int width=122,height=51,count=width*height;
        auto point=[](int i){return P(778+8*(i%width),109+8*(i/width));};
        for(int n=0;n<5;++n){
            beginSector(n,true);
            for(int open=0;open<2;++open){
                if(open)stacks.removeOne(shutter);
                QVector<char> seen(count,0);int start=29*width+5;QVector<int> q{start};seen[start]=1;
                for(int h=0;h<q.size();++h)for(int d:{-1,1,-width,width}){
                    int j=q[h]+d;if(j<0||j>=count||std::abs(j%width-q[h]%width)>1||seen[j]||!chapterFloor(point(j)))continue;
                    seen[j]=1;q.push_back(j);
                }
                auto reachable=[&](P at){for(int j:q)if(length(point(j)-at)<12)return true;return false;};
                if(!reachable(resonance)||reachable(exitPoint)!=bool(open)){qWarning()<<"layout"<<n<<open;return false;}
                if(open){
                    for(const auto &r:relays)if(!reachable(r.pos))return false;
                    for(const auto &g:guards)if(!reachable(g.home))return false;
                    if(medkit&&!reachable(kitPosition)){
                        qWarning()<<"Unreachable medkit in sector"<<n+1<<"at"<<kitPosition;
                        return false;
                    }
                }
            }
        }
        return true;
    }

    bool verifyBlindroomPlaythrough(const QString &path){
        auto fail=[this](int row){
            qWarning()<<"Formal route check:"<<row<<"room"<<sector<<"position"<<hero;
            return false;
        };
        auto key=[&](QEvent::Type type,int code){
            QKeyEvent event(type,code,Qt::NoModifier);
            QApplication::sendEvent(this,&event);
        };
        auto mouse=[&](QEvent::Type type,P world,Qt::MouseButton button,Qt::MouseButtons buttons){
            const double scale=scaleFactor();
            P view=(world-P(camera,cameraY()))*scale+P((width()-1000*scale)/2,(height()-615*scale)/2);
            QMouseEvent event(type,view,view,view,button,buttons,Qt::NoModifier);
            QApplication::sendEvent(this,&event);
        };
        auto stepTo=[&](P waypoint){
            const int initial=sector;
            for(int i=0;i<450&&!finished&&sector==initial&&length(hero-waypoint)>3.;++i){
                P d=waypoint-hero;
                QSet<int> wanted;
                if(std::abs(d.x())>2.)wanted.insert(d.x()>0?Qt::Key_D:Qt::Key_A);
                if(std::abs(d.y())>2.)wanted.insert(d.y()>0?Qt::Key_S:Qt::Key_W);
                const auto old=held;
                for(int k:old-wanted)key(QEvent::KeyRelease,k);
                for(int k:wanted-old)key(QEvent::KeyPress,k);
                tick(.016);
            }
            const auto old=held;for(int k:old)key(QEvent::KeyRelease,k);
            return finished||sector!=initial||length(hero-waypoint)<4.;
        };
        auto walk=[&](P goal){
            constexpr int w=124,h=54;auto point=[](int j){return P(777+8*(j%w),105+8*(j/w));};
            int start=qRound((hero.x()-777)/8)+w*qRound((hero.y()-105)/8);
            QVector<int> parent(w*h,-1),q{start};parent[start]=start;int end=-1;
            for(int k=0;k<q.size();++k){
                int v=q[k];if(length(point(v)-goal)<9){end=v;break;}
                for(int d:{1,-1,w,-w}){
                    int j=v+d;if(j<0||j>=w*h||std::abs(j%w-v%w)>1||parent[j]>=0)continue;bool free=true;for(P offset:{P(4,4),P(-4,4),P(4,-4),P(-4,-4)})free&=chapterFloor(point(j)+offset);if(!free)continue;
                    parent[j]=v;q.push_back(j);
                }
            }
            if(end<0){qWarning()<<"unreachable"<<goal;return false;}
            QVector<int> route;for(int j=end;j!=start;j=parent[j])route.prepend(j);
            int initial=sector;
            for(int i=0;i<route.size()&&initial==sector&&!finished;++i){
                if(i>0&&i+1<route.size()&&route[i+1]-route[i]==route[i]-route[i-1])continue;
                if(!stepTo(point(route[i]))){qWarning()<<"waypoint"<<point(route[i])<<"for"<<goal;return false;}
            }
            return initial!=sector||finished||stepTo(goal);
        };
        auto attack=[&](P aim){
            while(cooldown>0)tick(.016);
            mouse(QEvent::MouseButtonPress,aim,Qt::LeftButton,Qt::LeftButton);
            for(int i=0;i<13;++i)tick(.016);
            mouse(QEvent::MouseButtonPress,hero,Qt::RightButton,Qt::RightButton);
        };
        auto duel=[&](int index,P approach){
            for(int k=0;k<7&&guards[index].state!=4;++k){
                if(!walk(approach))return false;
                attack(guards[index].pos-P(0,5));
            }
            return guards[index].state==4;
        };
        for(int n=0;n<5;++n){
            key(QEvent::KeyPress,Qt::Key_Escape);
            mouse(QEvent::MouseButtonPress,floorButton(n).center(),Qt::LeftButton,Qt::LeftButton);
            if(page!=Playing||sector!=n||hearts!=3)return fail(__LINE__);
            pulse();for(int k=0;k<180;++k)tick(.016);
            if(hearts!=3||boss.active||passageFound)return fail(__LINE__);
            for(auto &g:guards)if(g.state!=0||g.pos!=g.home)return fail(__LINE__);
        }
        key(QEvent::KeyPress,Qt::Key_Escape);
        if(page!=Select)return fail(__LINE__);
        mouse(QEvent::MouseButtonPress,floorButton(0).center(),Qt::LeftButton,Qt::LeftButton);
        if(page!=Playing||sector!=0)return fail(__LINE__);
        auto scanNiche=[&](){
            if(!walk(resonance))return false;
            key(QEvent::KeyPress,Qt::Key_Space);key(QEvent::KeyRelease,Qt::Key_Space);
            for(int k=0;k<66;++k)tick(.016);
            if(passageFound||time<resonanceAt)return false;
            P nearest(1,0);double shortest=165;
            for(P d:{P(1,0),P(-1,0),P(0,1),P(0,-1)})for(int j=4;j<shortest;j+=4)
                if(!chapterFloor(hero+d*j)){nearest=d;shortest=j;break;}
            attack(hero+nearest*75);
            return passageFound&&walk(resonance);
        };
        if(!duel(0,{1010,469})||!walk({1000,419}))return fail(__LINE__);
        key(QEvent::KeyPress,Qt::Key_Space);key(QEvent::KeyRelease,Qt::Key_Space);
        for(int i=0;i<70;++i)tick(.016);
        if(!relays.front().seen)return fail(__LINE__);
        auto chargeRelay=[&](Relay&r){
            for(int x=799;x<=1400;x+=3){
                mouse(QEvent::MouseButtonPress,stageLamp,Qt::LeftButton,Qt::LeftButton);
                mouse(QEvent::MouseMove,{double(x),114},Qt::NoButton,Qt::LeftButton);
                mouse(QEvent::MouseButtonRelease,stageLamp,Qt::LeftButton,Qt::NoButton);
                if(powered(r)){for(int i=0;i<88;++i)tick(.016);if(r.cut||r.released<0)return false;
                    mouse(QEvent::MouseButtonPress,r.pos,Qt::LeftButton,Qt::LeftButton);
                    for(int i=0;i<15;++i)tick(.016);
                    mouse(QEvent::MouseButtonPress,hero,Qt::RightButton,Qt::RightButton);return r.cut;}
            }return false;
        };
        if(!chargeRelay(relays[0]))return fail(__LINE__);
        if(!walk({1070,419})||!walk({1070,150})||!walk({1280,148})||!duel(1,{1300,149}))return fail(__LINE__);
        if(!scanNiche()||!walk({1400,150})||!walk({1400,319})||!duel(2,{1490,316}))return fail(__LINE__);
        if(!walk({1660,315})||!walk(exitPoint))return fail(__LINE__);
        if(sector!=1)return fail(__LINE__);
        auto lampAt=[&](double x){
            mouse(QEvent::MouseButtonPress,stageLamp,Qt::LeftButton,Qt::LeftButton);
            mouse(QEvent::MouseMove,{x,114},Qt::NoButton,Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease,stageLamp,Qt::LeftButton,Qt::NoButton);
            return std::abs(stageLamp.x()-x)<1;
        };
        auto echoNow=[&](){
            key(QEvent::KeyPress,Qt::Key_Space);key(QEvent::KeyRelease,Qt::Key_Space);
            for(int i=0;i<65;++i)tick(.016);
        };
        // Lower maintenance aisle avoids the northern crossing. Sound bounces off
        // the southern boundary into the receiver before the cut is attempted.
        if(!scanNiche()||!lampAt(1040)||!walk({850,498})||!walk({1030,498})||!duel(1,{1035,474})||!walk({1515,498}))return fail(__LINE__);
        echoNow();
        if(!relays[0].seen){
            qWarning()<<"Sector 2: reflected echo did not reveal the relay";
            return fail(__LINE__);
        }
        if(!chargeRelay(relays[0]))return fail(__LINE__);if(!relays[0].cut)return fail(__LINE__);
        for(int x=799;x<=1400&&guards[0].armored;x+=5){
            if(!lampAt(x))return fail(__LINE__);
            if(litAt(guards[0].pos))
                for(int k=0;k<84;++k)tick(.016);
        }
        if(guards[0].armored)return fail(__LINE__);
        if(!lampAt(1395))return fail(__LINE__);
        for(int k=0;k<6&&guards[0].state!=4;++k){
            if(hero.y()<358&&(!walk({1435,hero.y()})||!walk({1435,430})))return fail(__LINE__);
            if(!walk({1405,434}))return fail(__LINE__);
            attack(guards[0].pos);
        }
        if(guards[0].state!=4||!walk({1515,463})||!walk({1683,463})||!walk({1683,313})||!duel(2,{1683,275}))return fail(__LINE__);
        if(sector==1&&medkit&&hearts<3&&!walk(kitPosition))return fail(__LINE__);
        if((sector==1&&!walk(exitPoint))||sector!=2)return fail(__LINE__);

        if(!scanNiche()||!channels().isEmpty()||!walk({1160,340}))return fail(__LINE__);
        echoNow();auto &seal=relays.first();
        if(!seal.seen||powered(seal))return fail(__LINE__);
        for(int i=0;i<2;++i){
            auto &light=projectors[i];
            mouse(QEvent::MouseButtonPress,light.source,Qt::LeftButton,Qt::LeftButton);
            const double x=i==0?1116:1384;
            mouse(QEvent::MouseMove,{x,114},Qt::NoButton,Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease,light.source,Qt::LeftButton,Qt::NoButton);
            if(!alignedShadow(i)||(i==0&&powered(seal)))return fail(__LINE__);
        }
        for(int j=0;j<95;++j)tick(.016);
        if(seal.released<0)return fail(__LINE__);
        grab().save(path+"/eclipse-seal.png");attack(seal.pos);
        if(!seal.cut||!walk({1378,458})||!walk({1520,458})||!walk(exitPoint))return fail(__LINE__);
        if(sector!=3||!scanNiche()||!duel(0,{968,301})||!duel(1,{1050,350}))return fail(__LINE__);
        if(!walk({1230,330}))return fail(__LINE__);
        for(int k=0;k<84;++k)tick(.016);
        if(wave!=1||!duel(2,{1352,279}))return fail(__LINE__);
        // The same crack is harmless whenever this guard returns to darkness.
        if(!guards[3].shadowbound)return fail(__LINE__);
        for(int x=799;x<=1400;x+=8){lampAt(x);if(!litAt(guards[3].pos))break;}
        int previousHp=guards[3].hp;
        cracks={QLineF(guards[3].pos-P(14,0),guards[3].pos+P(14,0))};cut();
        if(guards[3].hp!=previousHp)return fail(__LINE__);
        if(hearts<3&&!walk(kitPosition))return fail(__LINE__);
        for(int k=0;k<6&&guards[3].state!=4;++k){
            lampAt(1395);
            if(!walk({1410,395}))return fail(__LINE__);
            while(cooldown>0)tick(.016);
            mouse(QEvent::MouseButtonPress,guards[3].pos-P(0,5),Qt::LeftButton,Qt::LeftButton);
            for(int j=0;j<13;++j)tick(.016);
            for(int x=799;x<=1400;x+=4){lampAt(x);if(litAt(guards[3].pos))break;}
            mouse(QEvent::MouseButtonPress,hero,Qt::RightButton,Qt::RightButton);
        }
        if(guards[3].state!=4||!lampAt(1395)||!duel(4,{1485,325}))return fail(__LINE__);
        if(!walk(exitPoint)||sector!=4||(hearts<3&&!walk(kitPosition))||!scanNiche()||!walk({1250,355}))return fail(__LINE__);
        auto aimLamp=[&](P at){
            mouse(QEvent::MouseButtonPress,stageLamp,Qt::LeftButton,Qt::LeftButton);
            mouse(QEvent::MouseMove,at,Qt::NoButton,Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease,at,Qt::LeftButton,Qt::NoButton);
        };
        bool clearedClogs=false;
        for(int k=0;k<12000&&boss.hp>0;++k){
            if(!boss.globs.isEmpty()){
                if(boss.globs.size()==3&&!clearedClogs){
                    for(int j=0;j<100&&!lensBlocked();++j)tick(.016);
                    if(!lensBlocked()||!projectedLight().isEmpty())return fail(__LINE__);
                    grab().save(path+"/clogged-lamp.png");
                }
                P port=boss.globs.front().port;
                if(length(hero-port)>130&&!walk(port+P(0,-65)))return fail(__LINE__);
                attack(port);
                if(boss.globs.isEmpty())clearedClogs=true;
            }else if(boss.open>0){
                if(length(hero-boss.core)>137){
                    auto goal=boss.core+normalized(hero-boss.core)*90;if(!walk(goal))return fail(__LINE__);
                }
                attack(boss.core);
            }else{
                aimLamp(boss.core);
                tick(.016);
                if(boss.charging&&boss.flight>0&&length(hero-boss.core)<150&&cooldown<=0){
                    P dodge=hero+P(0,hero.y()>305?-130:130);mouse(QEvent::MouseButtonPress,dodge,Qt::LeftButton,Qt::LeftButton);
                }
                if(boss.active&&k==30)grab().save(path+"/warden.png");
            }
            if(!passageFound)return fail(__LINE__);
        }
        if(!clearedClogs)return fail(__LINE__);
        if(boss.hp||!walk(exitPoint))return fail(__LINE__);
        return finished;
    }

    bool check(QString path){
        timer.stop();mute=true;QDir().mkpath(path);
        auto mouse=[&](QEvent::Type type,P pos,Qt::MouseButton b,Qt::MouseButtons buttons){
            QMouseEvent event(type,pos,pos,pos,b,buttons,Qt::NoModifier);QApplication::sendEvent(this,&event);};
        grab().save(path+"/title.png");
        mouse(QEvent::MouseButtonPress,chooseButton.center(),Qt::LeftButton,Qt::LeftButton);
        if(page!=Select)return false;grab().save(path+"/select.png");
        mouse(QEvent::MouseButtonPress,course.center(),Qt::LeftButton,Qt::LeftButton);
        if(page!=Playing)return false;
        // Capture one cycle at game proportions for visual comparison.
        const P savedHero=hero;
        for(int i=0;i<12;++i){
            QImage pose(288,200,QImage::Format_ARGB32_Premultiplied);pose.fill(QColor("#090b13"));
            QPainter pp(&pose);pp.setRenderHint(QPainter::Antialiasing);
            hero={0,0};walkPhase=i*6.2831853/12;time=i*.05;held.insert(Qt::Key_D);
            pp.translate(76,150);pp.scale(3,3);drawHero(pp);
            Guard sample;sample.pos={85,0};sample.phase=walkPhase/7;
            pp.translate(P(45,0)-dummy);drawEnemy(pp,&sample);pp.end();
            pose.save(path+QString("/stride-%1.png").arg(i,2,10,QChar('0')));
        }
        for(int i=0;i<24;++i){
            QImage pose(184,132,QImage::Format_ARGB32_Premultiplied);pose.fill(QColor("#090b13"));QPainter pp(&pose);
            held.clear();walkPhase=i*.85;dashLeft=0;flash=0;
            if(i<7)held.insert(Qt::Key_D);
            if(i>=7&&i<11)dashLeft=.18-(i-7)*.04;
            if(i>=12&&i<21)flash=.36-(i-12)*.04;
            pp.translate(92,112);pp.scale(2.6,2.6);drawHero(pp);pp.end();
            pose.save(path+QString("/action-%1.png").arg(i,2,10,QChar('0')));
        }
        hero=savedHero;held.clear();walkPhase=time=0;
        auto frames=[&](int n){while(n--)tick(.016);};
        QKeyEvent down(QEvent::KeyPress,Qt::Key_D,Qt::NoModifier);QApplication::sendEvent(this,&down);frames(55);
        QKeyEvent up(QEvent::KeyRelease,Qt::Key_D,Qt::NoModifier);QApplication::sendEvent(this,&up);
        if(lesson!=Dash){
            qWarning()<<"Tutorial: reaching the movement marker did not start the dash lesson";
            return false;
        }
        if(!hitsGuard(dummy,QLineF(dummy+P(-45,-33),dummy+P(45,-33)))||
            hitsGuard(dummy,QLineF(dummy+P(-45,-65),dummy+P(45,-65))))return false;
        mouse(QEvent::MouseButtonPress,dummy,Qt::LeftButton,Qt::LeftButton);frames(21);
        if(lesson!=Sheathe||cracks.size()!=1){
            qWarning()<<"Tutorial: dash did not leave one crack and start the sheathe lesson";
            return false;
        }
        grab().save(path+"/fracture.png");
        QApplication::sendEvent(this,&down);frames(30);QApplication::sendEvent(this,&up);P cutPosition=hero;
        mouse(QEvent::MouseButtonPress,dummy,Qt::RightButton,Qt::RightButton);
        if(hero!=cutPosition||lesson!=Echo||!cracks.isEmpty()){
            qWarning()<<"Tutorial: sheathe did not consume the crack and start the echo lesson";
            return false;
        }
        frames(8);if(sparks.isEmpty()||hit<=0)return false;grab().save(path+"/sparks.png");
        auto walkTo=[&](P goal){
            for(int i=0;i<400&&length(hero-goal)>3;++i){
                P delta=goal-hero;held.clear();if(std::abs(delta.x())>2)held.insert(delta.x()>0?Qt::Key_D:Qt::Key_A);
                if(std::abs(delta.y())>2)held.insert(delta.y()>0?Qt::Key_S:Qt::Key_W);tick(.016);
            }held.clear();return length(hero-goal)<4;
        };
        QKeyEvent space(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier);QApplication::sendEvent(this,&space);frames(55);
        if(!walkTo({600,340})||!walkTo({600,270})||!walkTo({685,270})||!walkTo({685,340})||!walkTo({808,340}))return false;
        if(!chapter||chapterFloor({1800,350})||hearts!=3||chapterFloor({1151,340}))return false;
        frames(170);if(calibration||hearts!=2||!medkit)return false;
        auto clickWorld=[&](QEvent::Type t,P q,Qt::MouseButton b,Qt::MouseButtons bs){mouse(t,q-P(camera,0),b,bs);};
        auto &core=relays.front();
        clickWorld(QEvent::MouseButtonPress,stageLamp,Qt::LeftButton,Qt::LeftButton);
        clickWorld(QEvent::MouseMove,{950,114},Qt::NoButton,Qt::LeftButton);
        clickWorld(QEvent::MouseButtonRelease,stageLamp,Qt::LeftButton,Qt::NoButton);frames(90);
        if(core.released<0||core.cut||!walkTo({970,285}))return false;
        clickWorld(QEvent::MouseButtonPress,core.pos,Qt::LeftButton,Qt::LeftButton);frames(15);
        clickWorld(QEvent::MouseButtonPress,hero,Qt::RightButton,Qt::RightButton);
        if(!core.cut||!walkTo({808,340}))return false;
        clickWorld(QEvent::MouseButtonPress,stageLamp,Qt::LeftButton,Qt::LeftButton);
        clickWorld(QEvent::MouseMove,{1215,136},Qt::NoButton,Qt::LeftButton);
        clickWorld(QEvent::MouseButtonRelease,stageLamp,Qt::LeftButton,Qt::NoButton);
        QApplication::sendEvent(this,&down);frames(182);QApplication::sendEvent(this,&up);
        if(hero.x()<1282||medkit||hearts!=3)return false;
        for(int i=0;i<2;++i){
            frames(22);clickWorld(QEvent::MouseButtonPress,guards[i].pos,Qt::LeftButton,Qt::LeftButton);frames(14);
            clickWorld(QEvent::MouseButtonPress,hero,Qt::RightButton,Qt::RightButton);
        }
        if(!cleared())return false;
        grab().save(path+"/chapter.png");
        for(int i=0;i<250&&!finished;++i){P delta=exitPoint-hero;held.clear();
            if(std::abs(delta.x())>2)held.insert(delta.x()>0?Qt::Key_D:Qt::Key_A);
            if(std::abs(delta.y())>2)held.insert(delta.y()>0?Qt::Key_S:Qt::Key_W);tick(.016);}
        if(!finished)return false;
        beginSector(0,true);hero={862,345};frames(5);grab().save(path+"/blind-before.png");
        pulse();frames(38);grab().save(path+"/blind-after.png");
        if(heard.isEmpty())return false;
        auto mapFrame=[&](){
            QImage frame(1000,615,QImage::Format_ARGB32_Premultiplied);frame.fill(Qt::transparent);
            QPainter painter(&frame);echoMap(painter);painter.end();return frame;
        };
        auto savedContacts=heard;heard.clear();auto emptyMap=mapFrame();heard=savedContacts;
        const double scanTime=time;time+=13;
        if(mapFrame()==emptyMap)return false;
        time+=4;if(mapFrame()!=emptyMap)return false;time=scanTime;
        beginSector(1,true);
        auto &sentinel=guards.front();const QLineF blade(sentinel.pos-P(36,0),sentinel.pos+P(36,0));
        cracks={blade};cut();if(sentinel.hp!=2||!sentinel.armored)return false;
        for(int j=0;j<4;++j){slashStart=hero-P(20+j*3,0);finishDash();}
        if(cracks.size()!=3||retiredAt!=time||retiringCrack.length()!=20)return false;
        for(int n=0;n<2;++n){beginSector(n,true);for(auto &r:relays){
            hero=r.pos-P(30,0);waveStart=-20;pulse();frames(54);if(!r.seen)return false;
            for(int x=799;x<=1400;++x){stageLamp.setX(x);if(powered(r))break;}
            frames(90);if(r.cut||r.released<0)return false;cracks={{hero,r.pos}};cut();if(!r.cut)return false;
        }}
        beginSector(4,true);boss.active=true;boss.clock=10;boss.core={1530,410};
        boss.lightDir={0,-1};hero={1240,365};
        const P emitter(1500,365),west(-1,0);
        auto blocked=laserTrace(emitter,west);
        if(blocked.length()>210||blocked.length()<120)return false;
        boss.laserOrigin=emitter;boss.laserDir=west;boss.laserTime=.4;hearts=3;immunity=0;
        stepWarden(.016);if(hearts!=3||boss.open>0)return false;
        boss.lightDir=normalized(P(3,-.25));stepWarden(.016);
        if(hearts!=2||boss.open>0)return false;
        boss.hp=4;boss.volley=1;boss.winding=true;boss.clock=0;boss.aim=west;
        stepWarden(.016);if(boss.shots.size()<17)return false;
        grab().save(path+"/warden-barrage.png");
        return verifySectorNavigation()&&verifyBlindroomPlaythrough(path);
    }
};
int main(int argc,char**argv){QApplication app(argc,argv);Training game;
    if(app.arguments().size()==3&&app.arguments()[1]=="--verify"){game.show();return game.check(app.arguments()[2])?0:2;}game.showFullScreen();return app.exec();}
