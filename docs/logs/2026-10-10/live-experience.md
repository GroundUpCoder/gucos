# Live release status and user experience

Verified the actual 1.0.0 deployment and ran Playwright on the normal interactive
https://os.groundupcoder.net/ URL. Three fresh contexts and reloads, actual keyboard
installation of jsmn, JWM's X11 dependency closure and xterm, followed by start-jwm.
Every operation passed; no page errors. Retained exact measurement source/results,
original browser video, 12× GIF and MP4, and desktop screenshot under
`docs/research/live-experience-2026-10-10/`.

Fresh boot median 2.737 s; reload median 0.356 s. Single install observations:
jsmn 2.178 s total / 2.010 s first feedback; JWM 108.447 / 78.717 s; xterm after
JWM 27.891 / 16.105 s. Feedback excludes echo. Package downloads still need visible
progress; startup progress does not cover gucman. No runtime fix or new deployment
is claimed. An initial flawed probe using an unavailable clear command was stopped
and excluded; final probe checks new output as well as the visible terminal.

The existing external packages thread, 01a12401-30cf-7004-a932-af576a939e08
(Codex, metadata model gpt-6-astra), was asked to audit historical completeness.
That thread owns its separate package inventory and evidence. Its historical
record inspection is not a new execution of all historical applications.
