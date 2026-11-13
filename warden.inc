    struct Shard {P pos,dir;double speed=235;};
    struct Glob {P pos,port; bool stuck=false;};
    struct Warden {
        static constexpr int maxHp=8;
        P core{1440,295}, aim, absorption, destination;
        QVector<Shard> shots;
        QVector<Glob> globs;
        P lightDir=normalized(P(.31,-1)),laserOrigin,laserDir;
        QLineF laser;
        double heat=0,laserTime=0,grace=0;
        int hp=maxHp,volley=0;
        double clock=.7,open=0,absorbed=-20,flight=0;
        bool winding=false,active=false,charging=false;
    } boss;
    void chooseFlight(bool charge){
        boss.charging=charge;
        if(charge){
            boss.destination=boss.core+boss.aim*std::min(225.,length(hero-boss.core)+44);
        }else{
            const P perches[]={{1520,186},{1340,412},{1555,425},{1310,192}};
            auto point=perches[boss.volley%4];if(litAt(point))point.setX(point.x()-104);
            boss.destination=point;
        }
        boss.destination.setX(std::clamp(boss.destination.x(),1210.,1600.));
        boss.destination.setY(std::clamp(boss.destination.y(),155.,468.));
        boss.flight=length(boss.destination-boss.core)/(charge?580.:410.);
    }
    bool lensBlocked()const{
        return std::any_of(boss.globs.cbegin(),boss.globs.cend(),[](const Glob &g){return g.stuck;});
    }
    QLineF laserTrace(P from,P dir)const{
        const auto shield=projectedLight();
        for(double d=0;d<650;d+=3){
            P q=from+dir*d;
            if(!chapterFloor(q)||(shield.contains(q)&&!castShade(q)))return QLineF(from,q);
        }
        return QLineF(from,from+dir*650);
    }
    void deployGlobs(){
        double x=std::clamp(stageLamp.x(),1210.,1570.);
        for(P port:{P(x-56,465),P(x,420),P(x+56,465)})
            boss.globs.push_back({boss.core,port,false});
    }
    void stepWarden(double dt){
        if(!boss.hp)return;
        if(!boss.active){if(!passageFound||hero.x()<1155)return;boss.active=true;chooseFlight(false);}
        boss.open=std::max(0.,boss.open-dt);boss.grace=std::max(0.,boss.grace-dt);
        for(auto &g:boss.globs)if(!g.stuck){
            P delta=g.port-g.pos;
            if(length(delta)<dt*240+1){g.pos=g.port;g.stuck=true;}
            else g.pos+=normalized(delta)*(dt*240);
        }
        if(boss.open<=0&&boss.grace<=0){
            boss.heat=std::clamp(boss.heat+dt*(litAt(boss.core)?1.:-1.65),0.,1.45);
            if(boss.heat>=1.45){
                boss.heat=0;boss.open=2.7;boss.flight=0;boss.winding=false;
                boss.laserTime=0;boss.clock=.65;play(3);
            }
        }
        if(boss.laserTime>0){
            boss.laserTime-=dt;boss.laser=laserTrace(boss.laserOrigin,boss.laserDir);
            if(!litAt(hero)&&boss.laser.length()>1&&segmentDistance(hero,boss.laser)<8&&immunity<=0&&dashLeft<=0){
                loseHeart();immunity=1.3;
            }
        }
        if(boss.open<=0){
            if(boss.flight>0){
                double step=std::min(dt,boss.flight);boss.core+=(boss.destination-boss.core)*(step/boss.flight);
                boss.flight-=step;
                if(boss.charging&&length(hero-boss.core)<23&&dashLeft<=0&&immunity<=0){loseHeart();immunity=1.3;}
            }else if((boss.clock-=dt)<=0){
                if(!boss.winding){boss.aim=normalized(hero-boss.core);boss.winding=true;boss.clock=boss.hp<=4?.40:.55;}
                else{
                    double heading=std::atan2(boss.aim.y(),boss.aim.x());
                    if(boss.volley%3==0){
                        boss.laserTime=.48;boss.laserOrigin=boss.core;boss.laserDir=boss.aim;
                    }else{
                        int fan=boss.hp<=4?4:2;
                        for(int i=-fan;i<=fan;++i){double a=heading+i*.19;P d(std::cos(a),std::sin(a));
                            boss.shots.push_back({boss.core+d*24,d,250});}
                        if(boss.hp<=4){
                            for(int i=0;i<14;++i){double a=heading+(i+.5)*6.2831853/14;P d(std::cos(a),std::sin(a));
                                boss.shots.push_back({boss.core+d*28,d,154});}
                        }
                    }
                    ++boss.volley;boss.winding=false;boss.clock=.35;chooseFlight(boss.volley%3==0||litAt(boss.core));play(1);
                }
            }
        }
        for(int i=boss.shots.size()-1;i>=0;--i){
            auto &s=boss.shots[i];bool removed=false;
            int n=1+int(dt*s.speed/3);
            for(int j=0;j<n;++j){
                double ds=dt*s.speed/n;s.pos+=s.dir*ds;
                if(!chapterFloor(s.pos))removed=true;
                else if(litAt(s.pos)){
                    boss.absorbed=time;boss.absorption=s.pos;
                    removed=true;
                }else if(length(hero-s.pos)<12){
                    if(immunity<=0&&dashLeft<=0){loseHeart();immunity=1.3;}
                    removed=true;
                }
                if(removed)break;
            }
            if(removed)boss.shots.removeAt(i);
        }
    }
    void cutWarden(){
        for(int i=boss.globs.size()-1;i>=0;--i){
            for(const auto &blade:flashes)if(segmentDistance(boss.globs[i].pos,blade)<19){
                impactPoint=boss.globs[i].pos;ignite(blade);boss.globs.removeAt(i);break;
            }
        }
        if(!boss.hp||boss.open<=0)return;
        for(auto blade:flashes)if(segmentDistance(boss.core,blade)<27){
            --boss.hp;if(boss.hp==4)deployGlobs();boss.grace=.5;boss.heat=0;boss.open=0;boss.clock=.45;boss.charging=false;++boss.volley;
            if(boss.hp>0)chooseFlight(false);
            else{boss.globs.clear();boss.laserTime=0;boss.laser=QLineF();boss.flight=0;boss.winding=false;}
            boss.shots.clear();impactPoint=boss.core;ignite(blade);return;
        }
    }
    void paintWarden(QPainter &p){
        if(!boss.hp||!boss.active)return;
        QColor rose("#ff669c"),cyan("#94ecff");
        P c=boss.core;double sway=std::sin(time*3);
        p.save();p.translate(c);p.setBrush(QColor(15,10,25,200));p.setPen(Qt::NoPen);p.drawEllipse(P(0,23),53,12);
        for(int i=0;i<3;++i){
            int radius=72+i*8;QColor etch=i==1?QColor("#392337"):QColor("#29283b");
            p.setPen(QPen(etch,1));p.setBrush(Qt::NoBrush);
            for(int k=0;k<4;++k)p.drawArc(QRectF(-radius,-radius*.62,radius*2,radius*1.24),(k*90+10)*16,64*16);
        }
        for(int sign:{-1,1}){
            p.save();p.scale(sign,1);p.rotate((boss.open>0?-12:3)+sway*3);
            shape(p,{{10,-15},{26,-37},{59,-55},{50,-25},{61,-32},{46,-4},{27,8},{17,5}},QColor("#38243e"),rose);
            shape(p,{{20,-12},{50,-44},{40,-21},{28,-8}},QColor("#a34e85"),QColor("#f293b0"));
            shape(p,{{19,4},{44,-6},{36,15},{18,32},{24,12}},QColor("#29283c"),QColor("#897cac"));p.restore();
        }
        shape(p,{{0,-42},{10,-29},{18,-12},{13,7},{0,25},{-13,7},{-18,-12},{-10,-29}},QColor("#292538"),bone);
        for(int sign:{-1,1}){
            line(p,{sign*5.,-32},{sign*19.,-47+sway*2},rose,2);
            line(p,{sign*5.,-19},{sign*11.,-22},bone,2);
        }
        QColor rim=boss.open>0?cyan:rose;
        shape(p,{{0,-12},{8,0},{0,12},{-8,0}},rim,bone);
        if(boss.open<=0)ring(p,{0,0},24+2*sway,rose,2);

        if(boss.heat>0){p.setPen(QPen(amber,3));p.setBrush(Qt::NoBrush);p.drawArc(QRectF(-30,-30,60,60),90*16,-int(5760*boss.heat/1.45));}
        p.restore();
        for(const auto &g:boss.globs){
            if(g.stuck)line(p,g.pos,stageLamp,QColor(165,81,180,155),3);
            else ring(p,g.port,9,QColor("#956aa8"));
            p.setBrush(QColor("#8f3a82"));p.setPen(QPen(QColor("#ffa6e1"),2));
            p.drawEllipse(g.pos,13+std::sin(time*5),11);line(p,g.pos-P(5,3),g.pos-P(1,5),bone,2);
        }
        if(boss.laserTime>0&&boss.laser.length()>1){
            line(p,boss.laser.p1(),boss.laser.p2(),QColor(255,55,115,120),10);
            line(p,boss.laser.p1(),boss.laser.p2(),bone,2.2);ring(p,boss.laser.p2(),6,amber,2);
        }
        if(boss.winding){
            p.setPen(QPen(rose,1,Qt::DashLine));p.drawLine(boss.volley%3==0?laserTrace(c,boss.aim):QLineF(c,c+boss.aim*365));
            ring(p,c,28+boss.clock*18,rose,2);
        }
        for(const auto &s:boss.shots){
            P side(-s.dir.y(),s.dir.x());line(p,s.pos-s.dir*20,s.pos,QColor(255,86,152,95),5);
            shape(p,{s.pos+s.dir*9,s.pos+side*4,s.pos-s.dir*11,s.pos-side*4},rose,bone);
        }
        double age=time-boss.absorbed;
        if(age>=0&&age<.48){p.save();p.setOpacity(1-age/.48);
            ring(p,boss.absorption,6+age*69,cyan,2);p.restore();}
    }
