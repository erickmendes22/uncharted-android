package dev.unchartedg2.game;
import java.util.Locale;
/** Orbis system-language IDs consumed by sceSystemServiceParamGetInt in the guest. */
final class GameLanguage {
    static int id(String code,Locale system) {
        switch(code) {
            case "ja":return 0;case "fr":return "CA".equals(system.getCountry())?22:2;
            case "es":return "ES".equals(system.getCountry())?3:20;
            case "de":return 4;case "it":return 5;
            case "pt":return "PT".equals(system.getCountry())?7:17;
            case "ru":return 8;case "ko":return 9;
            case "zh":return "TW".equals(system.getCountry())||"HK".equals(system.getCountry())?10:11;
            case "pl":return 16;case "tr":return 19;default:return 1;
        }
    }
}
