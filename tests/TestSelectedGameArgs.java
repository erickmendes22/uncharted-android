package dev.unchartedg2.game;

public final class TestSelectedGameArgs {
    public static void main(String[] args) {
        for(int i=1;i<=3;i++) {
            String requested="u"+i;
            if(!SelectedGameArgs.valid(requested))throw new AssertionError(requested);
            if(!SelectedGameArgs.filename(requested).equals("args-"+requested+".txt"))
                throw new AssertionError("Wrong launch args: "+requested);
            int selected=0;
            for(int j=1;j<=3;j++)
                if(SelectedGameArgs.isSelected("u"+j,requested))selected=j;
            if(selected!=i)throw new AssertionError("U"+i+" launched U"+selected);
        }
        for(String bad:new String[]{"u4","U3","",null}) {
            if(SelectedGameArgs.valid(bad))throw new AssertionError("Accepted bad ID");
        }
        System.out.println("PASS: U1/U2/U3 select distinct boot arguments");
    }
}
