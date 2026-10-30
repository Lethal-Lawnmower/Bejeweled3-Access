// Hover-text agent: lists every instruction whose text contains one of the
// given substrings (e.g. "+ 0xcf0]"), with its function, so struct offsets
// that have no symbolic reference (fields of gApp etc.) can be traced.
// Args: OUT_PATH PATTERN...   (case-insensitive substring match)
//@category Bejeweled3Access
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;

public class HoverScan extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(a[0], "UTF-8");
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Instruction ins = it.next();
            String t = ins.toString().toLowerCase();
            for (int i = 1; i < a.length; i++) {
                if (t.contains(a[i].toLowerCase())) {
                    Function f = getFunctionContaining(ins.getAddress());
                    out.println(a[i] + "\t" + ins.getAddress() + "\t" +
                                (f == null ? "-" : f.getEntryPoint().toString()) + "\t" + ins);
                    break;
                }
            }
        }
        out.close();
    }
}
