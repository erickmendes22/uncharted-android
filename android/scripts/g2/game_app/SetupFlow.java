package dev.unchartedg2.game;

/** Permission continuation is consumed once; a denial never opens settings itself. */
final class SetupFlow {
    static final int NONE=0, SETUP=1, FOLDER=2, PKG=3, UPDATE=4, PLAY=5;
    static final int REQUEST=6, OFFER_SETTINGS=7, OPEN_SETTINGS=8;
    int pending=NONE;
    boolean requesting,waitingSettings,offeringSettings;
    boolean busy(){return requesting||waitingSettings||offeringSettings;}
    int begin(int action,boolean access){
        if(busy())return NONE;
        if(access)return action;
        pending=action;requesting=true;return REQUEST;
    }
    int permission(boolean access){
        if(!requesting)return NONE;
        requesting=false;
        if(access)return consume();
        offeringSettings=true;return OFFER_SETTINGS;
    }
    int openSettings(){
        if(!offeringSettings)return NONE;
        offeringSettings=false;waitingSettings=true;return OPEN_SETTINGS;
    }
    int resumed(boolean access){
        if(!waitingSettings)return NONE;
        waitingSettings=false;
        if(access)return consume();
        cancel();return NONE;
    }
    void cancel(){pending=NONE;requesting=false;waitingSettings=false;offeringSettings=false;}
    private int consume(){int result=pending;pending=NONE;return result;}
    static boolean access(int target,boolean allFiles,boolean read,boolean write){
        return allFiles||(target<30&&read&&write);
    }
}
