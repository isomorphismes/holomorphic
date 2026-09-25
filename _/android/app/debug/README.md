# Lasso sideload signing

`lasso-dev.p12` is intentionally non-secret and checked in. It exists only to give debug/sideload APKs a stable Android signing identity so CI builds update one another in place.

- alias: `lasso-dev`
- store/key password: `lasso-dev`
- certificate SHA-256: `27:EE:E6:9B:1C:BE:EB:C3:0E:AB:35:FE:7E:B5:45:9C:54:27:84:B4:CC:C6:84:53:A3:72:CB:68:02:CC:F7:CF`

Do not use this key for Play or production releases. Production signing remains separate.
