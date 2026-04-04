# Script Bundles

This directory stores reusable script bundles for copying into other projects.

## Available Bundle

- `vscode_portable`: Keil build, J-Link flash, RTT read, and RTT write workflow for VS Code on Windows.

## Local Tool

- `port_pack.py`: capture, diff, switch, and restore `*_port.c` / `*_port.h` project packs for this repository.

Running `python .\scripts\port_pack.py` without arguments opens a UI.

UI workflow:

- Select the current project so the existing workspace port files are saved back into that project pack first.
- Select the target project to apply its port files.
- Select `<blank>` as the target project to save the current project first and then delete workspace port files.

## Port Pack Manifest

`port_pack.py` supports an optional `manifest.json` at the root of each project pack.

Example:

```json
{
	"extraFiles": [
		"rep_config.h"
	]
}
```

Rules:

- `extraFiles` paths are repository-relative.
- Each extra file must also exist inside the project pack under the same relative path.
- `switch` and `restore` treat these files as part of the managed project state.

## Use

If you want to migrate the current VS Code tooling to another project or another computer, start from:

- `USER/Rep/scripts/vscode_portable/migration.md`

The actual files to copy are already prepared inside:

- `USER/Rep/scripts/vscode_portable/.vscode`
- `USER/Rep/scripts/vscode_portable/scripts`