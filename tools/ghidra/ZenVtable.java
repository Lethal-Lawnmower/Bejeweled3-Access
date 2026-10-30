// Zen options agent: lists vtable slots and decompiles the overridden ones.
// Args: OUT_PATH BASE_VTABLE N VTABLE... ; for each VTABLE prints slots
// 0..N-1, marks those differing from BASE_VTABLE and decompiles them.
//@category Bejeweled3Access
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.io.PrintWriter;

public class ZenVtable extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(a[0], "UTF-8");
        Address base = toAddr(a[1]);
        int n = Integer.parseInt(a[2]);
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        for (int i = 3; i < a.length; i++) {
            Address vt = toAddr(a[i]);
            out.println("//==== vtable " + vt);
            for (int s = 0; s < n; s++) {
                long p = getInt(vt.add(4L * s)) & 0xffffffffL;
                long b = getInt(base.add(4L * s)) & 0xffffffffL;
                boolean ov = p != b;
                out.printf("// slot %d (+0x%X) %08X %s%n", s, 4 * s, p, ov ? "OVERRIDE" : "");
            }
            for (int s = 0; s < n; s++) {
                long p = getInt(vt.add(4L * s)) & 0xffffffffL;
                long b = getInt(base.add(4L * s)) & 0xffffffffL;
                if (p == b) continue;
                Function f = getFunctionAt(toAddr(p));
                if (f == null) f = createFunction(toAddr(p), null);
                if (f == null) { out.println("//---- slot " + s + " no function"); continue; }
                DecompileResults r = d.decompileFunction(f, 60, monitor);
                out.printf("//---- slot %d (+0x%X) %s @ %s%n", s, 4 * s, f.getName(), f.getEntryPoint());
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed");
            }
        }
        out.close();
    }
}
