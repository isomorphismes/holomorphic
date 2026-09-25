# Stable sideload signing

The files whose names contain `lasso-dev` are retained only as a legacy Android signing identity. Keeping the exact key and debug package identity allows new random-holomorphic APKs to update older development installs in place; it does not indicate that the application still contains a lasso mode.

- legacy alias: `lasso-dev`
- store/key password: `lasso-dev`
- certificate SHA-256: `27:EE:E6:9B:1C:BE:EB:C3:0E:AB:35:FE:7E:B5:45:9C:54:27:84:B4:CC:C6:84:53:A3:72:CB:68:02:CC:F7:CF`

Do not use this key for Play or production releases. Production signing remains separate.
