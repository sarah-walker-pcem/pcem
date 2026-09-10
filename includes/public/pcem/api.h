#ifndef _PCEM_API_H_
#define _PCEM_API_H_

/* Import marker for everything that lives in libpcem-plugin-api.
 *
 * Without it, references from pcem into the plugin-api DLL are resolved by MinGW
 * auto-import: ld patches the 32-bit displacement of each call/access at startup
 * via runtime pseudo-relocations. Under -flto ld only sees the real relocations
 * after symbol resolution, so it cannot route the calls through import thunks and
 * emits thousands of those fixups instead - and a 32-bit fixup only works while
 * the DLL lands within +/-2GB of the exe. Once ASLR puts it further away,
 * _pei386_runtime_relocator aborts before main:
 *     32 bit pseudo relocation at ... out of range
 *
 * Marking the API dllimport routes every cross-DLL reference through __imp_,
 * which is position independent. The exporting side is deliberately left empty so
 * ld keeps auto-exporting the whole DLL as it does today.
 */

#if defined(_WIN32) && defined(PLUGIN_ENGINE) && !defined(pcem_plugin_api_EXPORTS)
#  define PCEM_API __declspec(dllimport)
#else
#  define PCEM_API
#endif

#endif /* _PCEM_API_H_ */
