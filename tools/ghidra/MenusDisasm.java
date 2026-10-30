// Prints disassembly listings for reverse-engineering notes (menus agent).
// Args: OUT_PATH then items:
//   ADDR            disassemble the whole function containing ADDR
//   ADDR:N          N instructions starting at ADDR
//   callers:ADDR    for every call to ADDR, print the containing function
//                   and the 12 instructions before the call (to recover
//                   pushed immediates and register setup such as EAX/ESI)
//   refs:ADDR       the same for every reference of any kind (data reads,
//                   writes, pushes of a string address)
//@category Bejeweled3Access
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class MenusDisasm extends GhidraScript {
    private PrintWriter out;

    private void print(Instruction ins) {
        out.println("  " + ins.getAddress() + "  " + ins);
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        out = new PrintWriter(args[0], "UTF-8");
        for (int i = 1; i < args.length; i++) {
            String a = args[i];
            if (a.startsWith("callers:") || a.startsWith("refs:")) {
                boolean callsOnly = a.startsWith("callers:");
                Address target = toAddr(a.substring(a.indexOf(':') + 1));
                out.println("//==== " + (callsOnly ? "callers" : "refs") + " of " + target);
                for (Reference ref : getReferencesTo(target)) {
                    if (callsOnly && !ref.getReferenceType().isCall()) continue;
                    Address from = ref.getFromAddress();
                    Function f = getFunctionContaining(from);
                    out.println("//---- call @ " + from + " in " + (f == null ? "?" : f.getName() + " @ " + f.getEntryPoint()));
                    List<Instruction> before = new ArrayList<>();
                    Instruction ins = getInstructionAt(from);
                    Instruction p = ins == null ? null : ins.getPrevious();
                    for (int k = 0; k < 12 && p != null; k++, p = p.getPrevious()) before.add(0, p);
                    for (Instruction b : before) print(b);
                    if (ins != null) print(ins);
                }
            } else if (a.contains(":")) {
                String[] parts = a.split(":");
                Instruction ins = getInstructionAt(toAddr(parts[0]));
                if (ins == null) ins = getInstructionAfter(toAddr(parts[0]));
                int n = Integer.parseInt(parts[1]);
                out.println("//==== " + a);
                for (int k = 0; k < n && ins != null; k++, ins = ins.getNext()) print(ins);
            } else {
                Function f = getFunctionContaining(toAddr(a));
                out.println("//==== function containing " + a + ": " + (f == null ? "none" : f.getName()));
                if (f == null) continue;
                for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) print(ins);
            }
        }
        out.close();
    }
}
