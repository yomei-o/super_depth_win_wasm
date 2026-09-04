// Define functions at the targets of the JMP table at 0x401005.
//
// The game reaches most of its routines through that table - it stores the
// address of a table entry in a global (DAT_004492c8, DAT_004bf164) and calls
// through the pointer - so nothing in the binary calls them directly and the
// analyser never made functions for them.  FUN_00414920 (the title menu) and
// FUN_00414b00 (the record screen) are two that were missing entirely from
// the decompiled dump.
//
//   analyzeHeadless <proj> sd -process superdepth.exe -noanalysis \
//       -scriptPath tools/ghidra -postScript MakeThunkFuncs.java
//
// Then run DumpAll.java again to get the C.
//
// @category SuperDepth
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;

public class MakeThunkFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        long first = 0x401005L;         // the table's first entry
        long last = 0x401220L;          // just past its end
        int made = 0, had = 0, bad = 0;

        for (long a = first; a < last; a += 5) {
            Address at = toAddr(a);
            if (!mem.contains(at)) continue;
            if ((mem.getByte(at) & 0xff) != 0xE9) continue;     // JMP rel32
            long target = a + 5 + (long) mem.getInt(at.add(1));
            Address t = toAddr(target);
            if (!mem.contains(t)) { bad++; continue; }

            Function f = getFunctionAt(t);
            if (f != null) { had++; continue; }

            // Disassemble first: some targets have never been looked at.
            if (getInstructionAt(t) == null) {
                DisassembleCommand cmd =
                    new DisassembleCommand(t, new AddressSet(t), true);
                cmd.applyTo(currentProgram, monitor);
            }
            f = createFunction(t, null);
            if (f == null) {
                println("could not make a function at " + t
                        + " (from " + at + ")");
                bad++;
            } else {
                println("made " + f.getName() + " at " + t
                        + " (from " + at + ")");
                made++;
            }
        }
        println("thunk targets: " + made + " new, " + had + " already there, "
                + bad + " no good");
    }
}
