// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Returns -1 for the regular GUI launch, otherwise a CLI exit status. */
int zavo_system_command(int argc, char **argv);

#ifdef __cplusplus
}
#endif
