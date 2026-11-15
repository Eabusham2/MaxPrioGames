# Steam cache classification

Installed Steam apps outside the built-in catalog can be classified from the
local `appcache/appinfo.vdf` file. Only `common/type = Game` qualifies. Applications,
tools, DLC, demos, missing metadata, and unknown formats do not qualify through
this path. The built-in catalog remains available without the cache.

The independent parser supports appinfo versions 40 and 41, following the format
references published by the authors of [SteamAppInfo](https://github.com/xPaw/SteamAppInfo#appinfovdf)
and [steam-vdf-parser](https://github.com/mexus/steam-vdf-parser#binary-format-reference).
No third-party parser or runtime component is bundled.

The file is opened read-only. Unrequested application payloads are skipped by
seeking; only the requested installed apps' records are read and interpreted.
For version 41, string-table entries are retained as one-byte classifications
for the few key names needed, instead of keeping their full text. Each requested
record's binary SHA-1 field is checked before its classification is accepted.
This detects local record corruption; SHA-1 is not used for authentication.

Bounds cap a requested payload at 8 MiB, all requested payloads in one call at
64 MiB, string-table entries at 1,048,576, and nested VDF objects at 32 levels.
Allocated payload memory is released after each record. There is no persistent
copy of the potentially large app cache. Discovery calls the classifier on its
existing refresh schedule, not on each process scan.

An unsupported version, malformed relevant metadata, failed requested-record
checksum, invalid offset, exceeded bound, or read error yields an empty result
silently. Unrequested payloads are not parsed or hashed. This intentionally
avoids a whole-file integrity scan and does not claim to validate unused records.

Portable fixtures exercise both versions, Game versus Application/Demo, nested
scope, duplicate keys, independent SHA-1 vectors, corrupt and truncated inputs,
invalid indexed keys and offsets, and seeking past a large unrequested record.
