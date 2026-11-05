# Holomorphic first-frame observation

The auxiliary launch lanes froze logcat four seconds after launch, before the checked renderer emitted its first-frame record. The product's separate generated-APK numerical, color and visual runs had passed at source 83acc7a7d5e73f2efdb79425e35c6f54f67a91f2; that did not satisfy the failing lane.

The maintained Grease helper starts a bounded threadtime logcat stream before launch, binds records to the actual launched PID, polls for the existing first-frame marker, retains the entire stream, and refuses a missing marker or process replacement. Existing zero/pole editing, motion, source provenance and crash assertions remain in their owning workflows. The original fixed-delay logic is retained in parent 83acc7a7d5e73f2efdb79425e35c6f54f67a91f2.

Type sketch: paths identify one log and screenshot; an observed numeric PID identifies one process; readiness is a bounded boolean observation of that process's actual record. Grease is used for the existing adb orchestration and qualified runtime. No compiler, renderer, shader, semantic threshold, installed application or public release changes are made. Hosted execution of this new helper is pending at commit creation; preceding results remain historical evidence.
