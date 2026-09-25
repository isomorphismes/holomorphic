#version 300 es
precision highp float;
precision highp int;

in vec2 v_ndc;
uniform vec2 u_resolution;
uniform int u_zero_count;
uniform int u_pole_count;
uniform vec2 u_zero_positions[32];
uniform vec2 u_pole_positions[32];
uniform vec2 u_holomorphic_coefficients[5];
uniform float u_remote_pole_time;
uniform float u_zoom;
layout(location = 0) out vec4 _idris_fragColor;

void main() {
  float _idris_t0 = u_resolution.x;
  float _idris_t1 = u_resolution.y;
  float _idris_t2 = min(_idris_t0, _idris_t1);
  float _idris_t3 = (0.42 * _idris_t2);
  float _idris_t4 = (_idris_t3 * u_zoom);
  float _idris_t5 = max(_idris_t4, 1e-6);
  float _idris_t6 = v_ndc.x;
  float _idris_t7 = (_idris_t6 * 0.5);
  float _idris_t9 = (_idris_t7 * _idris_t0);
  float _idris_t10 = v_ndc.y;
  float _idris_t11 = (_idris_t10 * 0.5);
  float _idris_t13 = (_idris_t11 * _idris_t1);
  vec2 _idris_t14 = vec2(_idris_t9, _idris_t13);
  float _idris_t15 = (1.0 / _idris_t5);
  vec2 _idris_t16 = (_idris_t15 * _idris_t14);
  vec2 _idris_t17 = (0.5 * u_resolution);
  float _idris_t18 = length(_idris_t17);
  float _idris_t19 = (_idris_t18 / _idris_t5);
  float _idris_t20 = float(u_zero_count);
  vec2 _idris_t21 = vec2(0.0, 0.0);
  vec2 _idris_t22 = _idris_t21;
  for (int _idris_t22_index = 0; _idris_t22_index < 32 && float(_idris_t22_index) < _idris_t20; ++_idris_t22_index) {
    vec2 _idris_t23 = u_zero_positions[int(float(_idris_t22_index))];
    vec2 _idris_t24 = (_idris_t16 - _idris_t23);
    float _idris_t25 = dot(_idris_t24, _idris_t24);
    float _idris_t26 = max(_idris_t25, 1e-16);
    float _idris_t27 = _idris_t24.y;
    float _idris_t28 = _idris_t24.x;
    float _idris_t29 = atan(_idris_t27, _idris_t28);
    float _idris_t30 = log(_idris_t26);
    float _idris_t31 = (0.5 * _idris_t30);
    vec2 _idris_t32 = vec2(_idris_t29, _idris_t31);
    vec2 _idris_t33 = (_idris_t22 + _idris_t32);
    float _idris_t34 = (float(_idris_t22_index) + 1.0);
    _idris_t22 = _idris_t33;
  }
  float _idris_t35 = float(u_pole_count);
  vec2 _idris_t37 = _idris_t21;
  for (int _idris_t37_index = 0; _idris_t37_index < 32 && float(_idris_t37_index) < _idris_t35; ++_idris_t37_index) {
    vec2 _idris_t38 = u_pole_positions[int(float(_idris_t37_index))];
    vec2 _idris_t39 = (_idris_t16 - _idris_t38);
    float _idris_t40 = dot(_idris_t39, _idris_t39);
    float _idris_t41 = max(_idris_t40, 1e-16);
    float _idris_t42 = _idris_t39.y;
    float _idris_t43 = _idris_t39.x;
    float _idris_t44 = atan(_idris_t42, _idris_t43);
    float _idris_t45 = log(_idris_t41);
    float _idris_t46 = (0.5 * _idris_t45);
    vec2 _idris_t47 = vec2(_idris_t44, _idris_t46);
    vec2 _idris_t48 = (_idris_t37 + _idris_t47);
    float _idris_t49 = (float(_idris_t37_index) + 1.0);
    _idris_t37 = _idris_t48;
  }
  vec2 _idris_t51 = _idris_t21;
  for (int _idris_t51_index = 0; _idris_t51_index < 24 && float(_idris_t51_index) < 24.0; ++_idris_t51_index) {
    float _idris_t52 = (float(_idris_t51_index) + 0.11);
    float _idris_t53 = (_idris_t52 * 127.1);
    float _idris_t54 = (_idris_t53 + 31.7);
    float _idris_t55 = sin(_idris_t54);
    float _idris_t56 = (_idris_t55 * 43758.5453123);
    float _idris_t57 = fract(_idris_t56);
    float _idris_t58 = (6.283185307179586 * _idris_t57);
    float _idris_t59 = cos(_idris_t58);
    float _idris_t60 = sin(_idris_t58);
    vec2 _idris_t61 = vec2(_idris_t59, _idris_t60);
    float _idris_t62 = _idris_t61.y;
    float _idris_t63 = _idris_t61.x;
    float _idris_t64 = atan(_idris_t62, _idris_t63);
    float _idris_t65 = (float(_idris_t51_index) + 7.91);
    float _idris_t66 = (_idris_t65 * 127.1);
    float _idris_t67 = (_idris_t66 + 31.7);
    float _idris_t68 = sin(_idris_t67);
    float _idris_t69 = (_idris_t68 * 43758.5453123);
    float _idris_t70 = fract(_idris_t69);
    bool _idris_t71 = (_idris_t70 < 0.5);
    bool _idris_t72;
    if (_idris_t71) {
      _idris_t72 = true;
    } else {
      _idris_t72 = false;
    }
    float _idris_t73;
    if (_idris_t72) {
      float _idris_t74 = (-1.0);
      _idris_t73 = _idris_t74;
    } else {
      _idris_t73 = 1.0;
    }
    float _idris_t75 = (float(_idris_t51_index) + 4.37);
    float _idris_t76 = (_idris_t75 * 127.1);
    float _idris_t77 = (_idris_t76 + 31.7);
    float _idris_t78 = sin(_idris_t77);
    float _idris_t79 = (_idris_t78 * 43758.5453123);
    float _idris_t80 = fract(_idris_t79);
    float _idris_t81 = mix(0.11, 0.19, _idris_t80);
    float _idris_t82 = (_idris_t73 * _idris_t81);
    float _idris_t83 = (_idris_t82 * u_remote_pole_time);
    float _idris_t84 = (_idris_t64 + _idris_t83);
    float _idris_t85 = (float(_idris_t51_index) + 11.23);
    float _idris_t86 = (_idris_t85 * 127.1);
    float _idris_t87 = (_idris_t86 + 31.7);
    float _idris_t88 = sin(_idris_t87);
    float _idris_t89 = (_idris_t88 * 43758.5453123);
    float _idris_t90 = fract(_idris_t89);
    float _idris_t91 = (6.283185307179586 * _idris_t90);
    float _idris_t92 = (2.0 * _idris_t84);
    float _idris_t93 = (_idris_t92 + _idris_t91);
    float _idris_t94 = sin(_idris_t93);
    float _idris_t95 = (0.22 * _idris_t94);
    float _idris_t96 = (float(_idris_t51_index) + 1.73);
    float _idris_t97 = (_idris_t96 * 127.1);
    float _idris_t98 = (_idris_t97 + 31.7);
    float _idris_t99 = sin(_idris_t98);
    float _idris_t100 = (_idris_t99 * 43758.5453123);
    float _idris_t101 = fract(_idris_t100);
    float _idris_t102 = mix(2.75, 4.0, _idris_t101);
    float _idris_t103 = (_idris_t102 + _idris_t95);
    float _idris_t104 = (-0.08);
    float _idris_t105 = (float(_idris_t51_index) + 14.67);
    float _idris_t106 = (_idris_t105 * 127.1);
    float _idris_t107 = (_idris_t106 + 31.7);
    float _idris_t108 = sin(_idris_t107);
    float _idris_t109 = (_idris_t108 * 43758.5453123);
    float _idris_t110 = fract(_idris_t109);
    float _idris_t111 = mix(_idris_t104, 0.08, _idris_t110);
    float _idris_t112 = (1.0 + _idris_t111);
    float _idris_t113 = cos(_idris_t84);
    float _idris_t114 = (_idris_t112 * _idris_t113);
    float _idris_t115 = (1.0 - _idris_t111);
    float _idris_t116 = sin(_idris_t84);
    float _idris_t117 = (_idris_t115 * _idris_t116);
    vec2 _idris_t118 = vec2(_idris_t114, _idris_t117);
    float _idris_t119 = (_idris_t19 * _idris_t103);
    vec2 _idris_t120 = (_idris_t119 * _idris_t118);
    vec2 _idris_t121 = (_idris_t16 - _idris_t120);
    float _idris_t122 = dot(_idris_t121, _idris_t121);
    float _idris_t123 = max(_idris_t122, 1e-16);
    float _idris_t124 = _idris_t121.y;
    float _idris_t125 = _idris_t121.x;
    float _idris_t126 = atan(_idris_t124, _idris_t125);
    float _idris_t127 = log(_idris_t123);
    float _idris_t128 = (0.5 * _idris_t127);
    vec2 _idris_t129 = vec2(_idris_t126, _idris_t128);
    vec2 _idris_t130 = (_idris_t51 + _idris_t129);
    float _idris_t131 = (float(_idris_t51_index) + 1.0);
    _idris_t51 = _idris_t130;
  }
  vec2 _idris_t132 = (_idris_t22 - _idris_t37);
  vec2 _idris_t133 = (_idris_t132 - _idris_t51);
  float _idris_t134 = (1.0 / 3.0);
  vec2 _idris_t135 = (_idris_t134 * _idris_t16);
  float _idris_t136 = _idris_t135.x;
  float _idris_t137 = _idris_t135.y;
  vec4 _idris_t138 = vec4(0.0, 0.0, _idris_t136, _idris_t137);
  vec4 _idris_t139 = _idris_t138;
  for (int _idris_t139_index = 0; _idris_t139_index < 5 && float(_idris_t139_index) < 5.0; ++_idris_t139_index) {
    float _idris_t140 = _idris_t139.x;
    float _idris_t141 = _idris_t139.y;
    vec2 _idris_t142 = vec2(_idris_t140, _idris_t141);
    float _idris_t143 = _idris_t139.z;
    float _idris_t144 = _idris_t139.w;
    vec2 _idris_t145 = vec2(_idris_t143, _idris_t144);
    vec2 _idris_t146 = u_holomorphic_coefficients[int(float(_idris_t139_index))];
    float _idris_t147 = _idris_t146.x;
    float _idris_t148 = _idris_t145.x;
    float _idris_t149 = (_idris_t147 * _idris_t148);
    float _idris_t150 = _idris_t146.y;
    float _idris_t151 = _idris_t145.y;
    float _idris_t152 = (_idris_t150 * _idris_t151);
    float _idris_t153 = (_idris_t149 - _idris_t152);
    float _idris_t156 = (_idris_t147 * _idris_t151);
    float _idris_t159 = (_idris_t150 * _idris_t148);
    float _idris_t160 = (_idris_t156 + _idris_t159);
    vec2 _idris_t161 = vec2(_idris_t153, _idris_t160);
    vec2 _idris_t162 = (_idris_t142 + _idris_t161);
    float _idris_t165 = (_idris_t148 * _idris_t136);
    float _idris_t168 = (_idris_t151 * _idris_t137);
    float _idris_t169 = (_idris_t165 - _idris_t168);
    float _idris_t172 = (_idris_t148 * _idris_t137);
    float _idris_t175 = (_idris_t151 * _idris_t136);
    float _idris_t176 = (_idris_t172 + _idris_t175);
    vec2 _idris_t177 = vec2(_idris_t169, _idris_t176);
    float _idris_t178 = _idris_t162.x;
    float _idris_t179 = _idris_t162.y;
    float _idris_t180 = _idris_t177.x;
    float _idris_t181 = _idris_t177.y;
    vec4 _idris_t182 = vec4(_idris_t178, _idris_t179, _idris_t180, _idris_t181);
    float _idris_t183 = (float(_idris_t139_index) + 1.0);
    _idris_t139 = _idris_t182;
  }
  float _idris_t184 = _idris_t139.x;
  float _idris_t185 = _idris_t139.y;
  vec2 _idris_t186 = vec2(_idris_t184, _idris_t185);
  float _idris_t187 = _idris_t133.x;
  float _idris_t188 = _idris_t186.y;
  float _idris_t189 = (_idris_t187 + _idris_t188);
  float _idris_t190 = _idris_t133.y;
  float _idris_t191 = _idris_t186.x;
  float _idris_t192 = (_idris_t190 + _idris_t191);
  float _idris_t193 = (_idris_t189 / 6.283185307179586);
  float _idris_t194 = floor(_idris_t193);
  float _idris_t195 = (_idris_t193 - _idris_t194);
  float _idris_t196 = (360.0 * _idris_t195);
  float _idris_t197 = (_idris_t192 / 2.302585092994046);
  float _idris_t198 = floor(_idris_t197);
  float _idris_t199 = (_idris_t197 - _idris_t198);
  float _idris_t200 = (4.0 * _idris_t199);
  float _idris_t201 = (66.0 + _idris_t200);
  float _idris_t202 = (_idris_t196 / 100.0);
  float _idris_t203 = floor(_idris_t202);
  float _idris_t204 = (_idris_t202 - _idris_t203);
  float _idris_t205 = (3.0 * _idris_t204);
  float _idris_t206 = (_idris_t201 + _idris_t205);
  float _idris_t207 = (_idris_t196 * 3.141592653589793);
  float _idris_t208 = (_idris_t207 / 180.0);
  float _idris_t209 = cos(_idris_t208);
  float _idris_t210 = (45.0 * _idris_t209);
  float _idris_t211 = sin(_idris_t208);
  float _idris_t212 = (45.0 * _idris_t211);
  bool _idris_t213 = (_idris_t206 > 8.0);
  bool _idris_t214;
  if (_idris_t213) {
    _idris_t214 = true;
  } else {
    _idris_t214 = false;
  }
  float _idris_t215;
  if (_idris_t214) {
    float _idris_t217 = (_idris_t206 + 16.0);
    float _idris_t218 = (_idris_t217 / 116.0);
    float _idris_t219 = pow(_idris_t218, 3.0);
    _idris_t215 = _idris_t219;
  } else {
    float _idris_t216 = (_idris_t206 / 903.2962962962963);
    _idris_t215 = _idris_t216;
  }
  float _idris_t220 = (13.0 * _idris_t206);
  float _idris_t221 = (_idris_t210 / _idris_t220);
  float _idris_t222 = (_idris_t221 + 0.19783982482140777);
  float _idris_t224 = (_idris_t212 / _idris_t220);
  float _idris_t225 = (_idris_t224 + 0.46833630293240974);
  float _idris_t226 = (9.0 * _idris_t215);
  float _idris_t227 = (_idris_t226 * _idris_t222);
  float _idris_t228 = (4.0 * _idris_t225);
  float _idris_t229 = (_idris_t227 / _idris_t228);
  float _idris_t230 = (3.0 * _idris_t222);
  float _idris_t231 = (12.0 - _idris_t230);
  float _idris_t232 = (20.0 * _idris_t225);
  float _idris_t233 = (_idris_t231 - _idris_t232);
  float _idris_t234 = (_idris_t215 * _idris_t233);
  float _idris_t236 = (_idris_t234 / _idris_t228);
  float _idris_t237 = (3.2404542 * _idris_t229);
  float _idris_t238 = (1.5371385 * _idris_t215);
  float _idris_t239 = (_idris_t237 - _idris_t238);
  float _idris_t240 = (0.4985314 * _idris_t236);
  float _idris_t241 = (_idris_t239 - _idris_t240);
  float _idris_t242 = (-0.969266);
  float _idris_t243 = (_idris_t242 * _idris_t229);
  float _idris_t244 = (1.8760108 * _idris_t215);
  float _idris_t245 = (_idris_t243 + _idris_t244);
  float _idris_t246 = (0.041556 * _idris_t236);
  float _idris_t247 = (_idris_t245 + _idris_t246);
  float _idris_t248 = (0.0556434 * _idris_t229);
  float _idris_t249 = (0.2040259 * _idris_t215);
  float _idris_t250 = (_idris_t248 - _idris_t249);
  float _idris_t251 = (1.0572252 * _idris_t236);
  float _idris_t252 = (_idris_t250 + _idris_t251);
  float _idris_t253 = max(_idris_t241, 0.0);
  bool _idris_t254 = (_idris_t253 <= 0.0031308);
  bool _idris_t255;
  if (_idris_t254) {
    _idris_t255 = true;
  } else {
    _idris_t255 = false;
  }
  float _idris_t256;
  if (_idris_t255) {
    float _idris_t261 = (12.92 * _idris_t253);
    _idris_t256 = _idris_t261;
  } else {
    float _idris_t257 = (1.0 / 2.4);
    float _idris_t258 = pow(_idris_t253, _idris_t257);
    float _idris_t259 = (1.055 * _idris_t258);
    float _idris_t260 = (_idris_t259 - 0.055);
    _idris_t256 = _idris_t260;
  }
  float _idris_t262 = clamp(_idris_t256, 0.0, 1.0);
  float _idris_t263 = max(_idris_t247, 0.0);
  bool _idris_t264 = (_idris_t263 <= 0.0031308);
  bool _idris_t265;
  if (_idris_t264) {
    _idris_t265 = true;
  } else {
    _idris_t265 = false;
  }
  float _idris_t266;
  if (_idris_t265) {
    float _idris_t271 = (12.92 * _idris_t263);
    _idris_t266 = _idris_t271;
  } else {
    float _idris_t267 = (1.0 / 2.4);
    float _idris_t268 = pow(_idris_t263, _idris_t267);
    float _idris_t269 = (1.055 * _idris_t268);
    float _idris_t270 = (_idris_t269 - 0.055);
    _idris_t266 = _idris_t270;
  }
  float _idris_t272 = clamp(_idris_t266, 0.0, 1.0);
  float _idris_t273 = max(_idris_t252, 0.0);
  bool _idris_t274 = (_idris_t273 <= 0.0031308);
  bool _idris_t275;
  if (_idris_t274) {
    _idris_t275 = true;
  } else {
    _idris_t275 = false;
  }
  float _idris_t276;
  if (_idris_t275) {
    float _idris_t281 = (12.92 * _idris_t273);
    _idris_t276 = _idris_t281;
  } else {
    float _idris_t277 = (1.0 / 2.4);
    float _idris_t278 = pow(_idris_t273, _idris_t277);
    float _idris_t279 = (1.055 * _idris_t278);
    float _idris_t280 = (_idris_t279 - 0.055);
    _idris_t276 = _idris_t280;
  }
  float _idris_t282 = clamp(_idris_t276, 0.0, 1.0);
  vec3 _idris_t283 = vec3(_idris_t262, _idris_t272, _idris_t282);
  float _idris_t284 = _idris_t283.x;
  float _idris_t285 = _idris_t283.y;
  float _idris_t286 = _idris_t283.z;
  vec4 _idris_t287 = vec4(_idris_t284, _idris_t285, _idris_t286, 1.0);
  _idris_fragColor = _idris_t287;
}
