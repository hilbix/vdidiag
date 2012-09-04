A VirtualBox VDI file check utility
===================================

Because I needed some tool to verify that the trouble at my side was not caused by a defective VDI file structure.


Usage:
------

```bash
make
./vdicheck -v IMAGE.vdi
```

What it does:
-------------

- Read the header of an VDI file and checks it for obvious magics.
- Read the gaps in the VDI file and look if they are filled with NUL
- Read the block chain and verify that it is correct
- Output some diagnostics, with -v also show empty fields
- If verification fails, program returns false


What still is missing:
----------------------

- Report bad alignments, the blocks should be alinged on 4K boundaries for modern SSD drives
- Grok others than the two type of VDI files I saw
- Make sense of the "unknown2" field, for now it just dumps it
- Make sense of all the other UUIDs
- Verify UUID chains (like snapshots or immutable disks)
- Repair defective block chains
- Defragment VDI files
- Align VDI files
- Convert dynamic VDI files into fixed and vice versa


Known bugs:
-----------

- Some header fields are unclear to me as I did not dive into the VirtualBox source code yet.  Also the code was only tested with the VDI files I had.

