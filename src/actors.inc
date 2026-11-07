    void pixelActor(QPainter&p,P foot,bool enemy,bool left,double gait,int pose){
        static const char* runner[]={
            "    vviiiii    ","   vviiiihhi   ","  vviihhhhhhii ","  viiikkkkkki  ","   viiiaaaak   ","    vviikkk    ",
            " ppprrrrrr     ","pp  vviiiiv    ","   vvviviivk   ","   vvviivvka   ","  vvvviivv k a ","    vvvvv   a  "};
        static const char* sentinel[]={
            "   o      o    ","   voiiiivo    ","  vvviiiiiii   "," vvikkkkkkiiv  ","  vikoooookiv  ","   vviivviiv   ",
            " vviivvvvviiiv ","viihivvoovihhiv","viihivvovvihhiv"," vviiviivvvivo ","  vvvviivvvvv  ","    vvvvvvv    "};
        p.save();p.translate(foot);if(left)p.scale(-1,1);
        p.scale(enemy?2.9:2.55,enemy?2.9:2.55);p.setRenderHint(QPainter::Antialiasing,false);
        int step=qRound(gait*2);p.fillRect(QRect(-4-step,-2,3,4),QColor("#615570"));p.fillRect(QRect(1+step,-2,3,4),QColor("#a696b2"));
        p.translate(pose==2?2:0,pose==1?1:-(std::abs(step)==2));
        for(int y=0;y<12;++y)for(int x=0;x<15;++x){
            char k=(enemy?sentinel:runner)[y][x];if(k==' ')continue;
            QColor c=k=='i'?QColor(enemy?"#a48094":"#beb4de"):k=='h'?QColor("#ede0ef"):k=='a'?QColor("#79eafb"):k=='o'?QColor(pose==1?"#fff0be":"#ff8f79"):k=='p'||k=='r'?QColor("#fd76ad"):k=='k'?QColor("#272638"):QColor("#635475");
            p.fillRect(QRect(x-7,y-13,1,1),c);}
        p.restore();
    }
    // Draw into a small native pixel canvas. Boots, mantle and sword share the
    // same pivot, so mirrored frames keep the feet on the collision point.
    void drawHero(QPainter &p){
        QImage frame(64,48,QImage::Format_ARGB32_Premultiplied);
        frame.fill(Qt::transparent);QPainter q(&frame);q.translate(32,42);
        if(facesLeft)q.scale(-1,1);
        auto poly=[&](const char *c,std::initializer_list<QPoint> v){
            QPolygon pts;for(auto a:v)pts<<a;q.setPen(Qt::NoPen);q.setBrush(QColor(c));q.drawPolygon(pts);};
        auto stroke=[&](QPoint a,QPoint b,const char *c,int w){
            q.setPen(QPen(QColor(c),w,Qt::SolidLine,Qt::SquareCap));q.drawLine(a,b);};
        const bool dashing=dashLeft>0, cutting=flash>0&&!dashing;
        const double t=1-std::clamp(flash/.36,0.,1.);
        const double stride=held.isEmpty()?0:std::sin(walkPhase);
        const int step=qRound(4*stride);
        const int dashBeat=dashLeft>.13?0:dashLeft>.045?1:2;
        int lean=dashing?(dashBeat==0?2:dashBeat==1?7:4):cutting?qRound(3*std::sin(t*6)):0;
        int crouch=dashing?(dashBeat==0?7:dashBeat==1?5:3):cutting?qRound(3*std::sin(t*3.14)):std::abs(step)/3;
        // Split boots and a stepping knee are visible beneath the short coat.
        QPoint back(dashing?(dashBeat==1?-13:-6):-3-step,0),front(dashing?(dashBeat==0?3:8):3+step,0);
        stroke({-2,-9+crouch},back,"#43445f",4);stroke(back,back+QPoint(3,0),"#9184a2",2);
        stroke({2,-9+crouch},front,"#8985a3",4);stroke(front,front+QPoint(3,0),"#e1cddd",2);
        q.translate(lean,crouch);
        const int tail=dashing?12:cutting?7:std::abs(step);
        poly("#ac377c",{{-4,-21},{-10,-18},{-15-tail,-21},{-12-tail,-16},{-7,-15},{0,-19}});
        poly("#fa75ae",{{-4,-20},{-9,-16},{-12-tail,-13},{-16-tail,-14},{-13-tail,-11},{-7,-12},{0,-18}});
        poly("#302e49",{{-5,-20},{3,-19},{6,-12},{8,-7},{2,-8},{0,-6},{-7,-8},{-9,-7}});
        poly("#746789",{{-4,-19},{0,-18},{-2,-10},{-8,-8}});
        poly("#c9acc0",{{0,-18},{4,-18},{5,-13},{2,-11},{-2,-13}});
        stroke({-5,-8},{-13,-4},"#8585af",2);
        poly("#504969",{{-7,-27},{-3,-32},{3,-32},{8,-27},{8,-21},{4,-18},{-4,-19},{-8,-22}});
        poly("#e2d7e5",{{-7,-26},{-4,-30},{1,-32},{5,-30},{8,-26},{4,-26},{2,-28},{-3,-24},{-7,-22}});
        poly("#262b43",{{-3,-24},{7,-26},{8,-23},{4,-20},{-2,-20}});
        stroke({2,-23},{7,-24},"#93faff",2);
        stroke({-4,-19},{2,-18},"#ff83bc",2);
        QPoint hand(5,-12),tip(12,-5);
        if(dashing){hand={4,-13};tip=dashBeat==0?QPoint(-13,-3):dashBeat==1?QPoint(-23,-9):QPoint(-15,-19);}
        if(cutting){
            // Lift, sweep across the silhouette, then feed the blade into the hip scabbard.
            if(t<.30){hand={6,-16};tip={14,-31};}
            else if(t<.68){hand={5,-12};tip={23,-15+qRound(14*(t-.3))};}
            else{hand={-2,-10};tip={-5-qRound(8*(1-t)/.32),-7};}
        }
        stroke({1,-17},hand,"#9284a1",3);
        stroke(hand,tip,"#94cbd9",2);stroke(hand+QPoint(1,-1),tip,"#f5f5ef",1);
        stroke(hand+QPoint(-1,-1),hand+QPoint(1,1),"#ff9ab9",2);
        if(cutting&&t>.22&&t<.58){
            poly("#e2f6ff",{{11,-30},{18,-25},{24,-17},{25,-12},{20,-20}});
        }
        q.end();p.save();p.setRenderHint(QPainter::SmoothPixmapTransform,false);
        p.drawImage(QRectF(hero-P(32,42)*1.18,QSizeF(64*1.18,48*1.18)),frame);p.restore();
    }
    void drawEnemy(QPainter&p,const Guard *g=nullptr){pixelActor(p,dummy,true,g&&hero.x()<g->pos.x(),g&&g->state==0?std::sin(g->phase*7):0,g?g->state:0);}
