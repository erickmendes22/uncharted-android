package dev.unchartedg2.game;
import java.util.Locale;
public final class TestGameLanguage {
 public static void main(String[] args) {
  if(GameLanguage.id("pt",new Locale("pt","BR"))!=17)throw new AssertionError("Brazilian Portuguese");
  if(GameLanguage.id("pt",new Locale("pt","PT"))!=7)throw new AssertionError("European Portuguese");
  if(GameLanguage.id("pl",Locale.ROOT)!=16 || GameLanguage.id("tr",Locale.ROOT)!=19)throw new AssertionError("Polish/Turkish");
  if(GameLanguage.id("unsupported",Locale.ROOT)!=1)throw new AssertionError("English fallback");
  String[] codes={"en","es","fr","de","it","ru","ja","ko","zh"};int[] ids={1,20,2,4,5,8,0,9,11};
  for(int i=0;i<codes.length;i++)if(GameLanguage.id(codes[i],Locale.ROOT)!=ids[i])throw new AssertionError(codes[i]);
  System.out.println("Guest language mapping: PASS (12 supported UI languages and English fallback)");
 }
}
