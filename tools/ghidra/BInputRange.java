// Board-input agent: prints instructions in address ranges, read-only.
// Args: OUT_PATH then items: "LO-HI" (hex) prints instructions in that
// range; "str:ADDR" prints the data/string defined at ADDR (or 64 raw bytes).
//@category Bejeweled3Access
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Instruction;

import java.io.PrintWriter;

public class BInputRange extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(a[0], "UTF-8");
        for (int i = 1; i < a.length; i++) {
            if (a[i].startsWith("str:")) {
                Address ad = toAddr(a[i].substring(4));
                Data d = getDataAt(ad);
                out.println("//str " + ad + " : " + (d == null ? "?" : d.getDefaultValueRepresentation()));
                byte[] b = getBytes(ad, 64);
                StringBuilder sb = new StringBuilder();
                for (byte x : b) sb.append(x >= 32 && x < 127 ? (char) x : '.');
                out.println("//raw " + sb);
                continue;
            }
            String[] r = a[i].split("-");
            Address lo = toAddr(r[0]), hi = toAddr(r[1]);
            out.println("//==== " + lo + "-" + hi + " in " + getFunctionContaining(lo));
            Instruction ins = getInstructionAt(lo);
            if (ins == null) ins = getInstructionAfter(lo);
            while (ins != null && ins.getAddress().compareTo(hi) <= 0) {
                out.println(ins.getAddress() + "  " + ins);
                ins = ins.getNext();
            }
        }
        out.close();
    }
}
