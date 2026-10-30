// Game-actions agent: finds instructions whose text contains a substring.
// Args: OUT_PATH [range:LO-HI] PATTERN... ; for each match prints the
// containing function and the 8 instructions before it. Optional range
// limits the search to addresses LO..HI (hex).
//@category Bejeweled3Access
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class ActionsGrep extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(a[0], "UTF-8");
        long lo = 0, hi = Long.MAX_VALUE;
        List<String> pats = new ArrayList<>();
        for (int i = 1; i < a.length; i++) {
            if (a[i].startsWith("range:")) {
                String[] r = a[i].substring(6).split("-");
                lo = Long.parseLong(r[0], 16);
                hi = Long.parseLong(r[1], 16);
            } else pats.add(a[i]);
        }
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            long off = ins.getAddress().getOffset();
            if (off < lo || off > hi) continue;
            String s = ins.toString();
            for (String p : pats) {
                if (!s.contains(p)) continue;
                Function f = getFunctionContaining(ins.getAddress());
                out.println("//---- " + p + " @ " + ins.getAddress() + " in " + (f == null ? "?" : f.getName()));
                List<Instruction> before = new ArrayList<>();
                Instruction q = ins.getPrevious();
                for (int k = 0; k < 8 && q != null; k++, q = q.getPrevious()) before.add(0, q);
                for (Instruction b : before) out.println("  " + b.getAddress() + "  " + b);
                out.println("  " + ins.getAddress() + "  " + ins);
                break;
            }
        }
        out.close();
    }
}
