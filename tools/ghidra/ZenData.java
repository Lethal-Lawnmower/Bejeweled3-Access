// Zen options agent: prints data values. Args: OUT_PATH then items
//   d:ADDR (double), f:ADDR (float), i:ADDR (int32), u:ADDR (UTF-16 string)
//@category Bejeweled3Access
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import java.io.PrintWriter;

public class ZenData extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(a[0], "UTF-8");
        for (int i = 1; i < a.length; i++) {
            String[] p = a[i].split(":");
            Address ad = toAddr(p[1]);
            switch (p[0]) {
                case "d": out.println(a[i] + " = " + Double.longBitsToDouble(getLong(ad))); break;
                case "f": out.println(a[i] + " = " + Float.intBitsToFloat(getInt(ad))); break;
                case "i": out.printf("%s = 0x%08X%n", a[i], getInt(ad)); break;
                default: {
                    StringBuilder sb = new StringBuilder();
                    for (int k = 0; k < 200; k++) { char c = (char) (getShort(ad.add(2L * k)) & 0xffff); if (c == 0) break; sb.append(c); }
                    out.println(a[i] + " = \"" + sb + "\"");
                }
            }
        }
        out.close();
    }
}
