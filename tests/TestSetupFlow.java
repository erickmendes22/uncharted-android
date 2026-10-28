package dev.unchartedg2.game;
public final class TestSetupFlow {
    static void require(boolean value){if(!value)throw new AssertionError();}
    public static void main(String[] args){
        require(SetupFlow.access(28,false,true,true));
        require(!SetupFlow.access(28,false,true,false));
        require(!SetupFlow.access(35,false,true,true));
        require(SetupFlow.access(35,true,false,false));
        for(int action=SetupFlow.SETUP;action<=SetupFlow.PLAY;action++){
            SetupFlow f=new SetupFlow();
            require(f.begin(action,true)==action&&!f.busy());
            require(f.begin(action,false)==SetupFlow.REQUEST);
            require(f.begin(SetupFlow.PKG,false)==SetupFlow.NONE); // no duplicate prompt
            require(f.resumed(false)==SetupFlow.NONE&&f.requesting); // runtime dialog has its own callback
            require(f.permission(true)==action);
            require(f.permission(true)==SetupFlow.NONE&&f.pending==SetupFlow.NONE&&!f.busy());
            require(f.begin(action,false)==SetupFlow.REQUEST);
            require(f.permission(false)==SetupFlow.OFFER_SETTINGS);
            require(f.offeringSettings&&!f.waitingSettings); // denial does not launch settings
            require(f.openSettings()==SetupFlow.OPEN_SETTINGS);
            require(f.resumed(true)==action);
            require(f.resumed(true)==SetupFlow.NONE&&!f.busy());
            require(f.begin(action,false)==SetupFlow.REQUEST);
            require(f.permission(false)==SetupFlow.OFFER_SETTINGS);
            require(f.openSettings()==SetupFlow.OPEN_SETTINGS);
            require(f.resumed(false)==SetupFlow.NONE&&!f.busy());
            require(f.resumed(false)==SetupFlow.NONE); // refusal never loops
            f.begin(action,false);f.permission(false);f.cancel();
            require(!f.busy()&&f.pending==SetupFlow.NONE);
        }
        // Fields retained by Bundle restoration continue the pending picker once.
        SetupFlow restored=new SetupFlow();restored.pending=SetupFlow.FOLDER;restored.waitingSettings=true;
        require(restored.resumed(true)==SetupFlow.FOLDER);
        require(restored.resumed(true)==SetupFlow.NONE);
        System.out.println("PASS: legacy access, grant/deny/retry, one-shot continuation and no permission/settings loop");
    }
}
