# TankEye-Iris 1.4.2 缂栬瘧銆佸惎鍔ㄣ€佹墦鍖呮祦绋?

鏈枃妗ｆ寜褰撳墠椤圭洰绾﹀畾鏁寸悊銆備互鍚庣粺涓€浣跨敤 `build` 鐩綍锛屼笉鍐嶄娇鐢?`build_qt_codex`銆乣build_repackage` 绛変复鏃剁洰褰曘€俈isual Studio 鐢熸垚鍣ㄦ槸澶氶厤缃瀯寤猴紝Release 绋嬪簭鐢熸垚鍦細

```text
build\Release\tankeye-openvino_qt_app.exe
```

涓嶆槸锛?

```text
build\tankeye-openvino_qt_app.exe
```

## 1. 杩涘叆椤圭洰鐩綍

```powershell
cd D:\work_floder\jiezhifa\TankEye_source_for_new_pc
```

## 2. 棣栨閰嶇疆 build

濡傛灉 `build` 宸茬粡瀛樺湪涓旈厤缃纭紝鍙互璺宠繃鏈楠ゃ€傞噸鏂颁粠闆跺紑濮嬫椂锛屽厛娓呯悊鏃?build锛岀劧鍚庢墽琛岋細

```powershell
cmake -S . -B build `
  -DBUILD_QT_APP=ON `
  -DOpenVINO_DIR="D:\Anaconda\envs\cll_yolo\Lib\site-packages\openvino\cmake" `
  -DOpenCV_DIR="D:\work_floder\jiezhifa\TankEye_source_for_new_pc\_deps\opencv\opencv\build\x64\vc16\lib" `
  -DQt5_DIR="D:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5"
```

閰嶇疆鎴愬姛鍚庯紝`build` 鐩綍閲屼細鏈?CMake 鐢熸垚鐨?Visual Studio 宸ョ▼鏂囦欢銆?

## 3. 缂栬瘧 Release 绋嬪簭

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
cmake --build build --config Release --target tankeye-admin-auth-code
```

妫€鏌ョ▼搴忔槸鍚﹀瓨鍦細

```powershell
Test-Path .\build\Release\tankeye-openvino_qt_app.exe
```

杩斿洖 `True` 璇存槑涓荤▼搴忓凡缁忕紪璇戝嚭鏉ャ€?

## 4. 缂栬瘧骞惰繍琛屽叧閿祴璇?

鍙獙璇佹姄鍙栧悗澶勭悊閫昏緫锛?

```powershell
cmake --build build --config Release --target tankeye-openvino_frame_postprocess_smoke
```

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
```

杩愯鍏ㄩ儴娴嬭瘯锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release
```

鐪嬪埌锛?

```text
[TankEyeTests] All tests passed.
```

璇存槑娴嬭瘯閫氳繃銆?

## 5. 浠庢簮鐮佺洰褰曞惎鍔ㄧ▼搴?

甯哥敤璋冭瘯鍚姩鍛戒护锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -BuildDir build -Configuration Release -WindowMode Maximized -AutoLoadModels -DebugPostprocess -SimulatePlc
```

鍙傛暟璇存槑锛?

- `-BuildDir build`锛氫娇鐢ㄧ粺涓€鐨?`build` 缂栬瘧鐩綍銆?
- `-Configuration Release`锛氬惎鍔?`build\Release` 涓嬬殑绋嬪簭銆?
- `-WindowMode Maximized`锛氭渶澶у寲绐楀彛銆?
- `-AutoLoadModels`锛氳嚜鍔ㄥ姞杞?`models\weights` 涓嬬殑 OBB 鍜?SEG 妯″瀷銆?
- `-DebugPostprocess`锛氭墦寮€鍚庡鐞嗚皟璇曟棩蹇楋紝鏂逛究鐪嬩负浠€涔堝彲鎶撴垨涓嶅彲鎶撱€?
- `-SimulatePlc`锛歅LC 妯℃嫙妯″紡锛屼笉鐪熷疄鍐?PLC銆?

濡傛灉瑕佽繛鎺ョ湡瀹?PLC锛屼笉瑕佸姞 `-SimulatePlc`锛屽苟纭 `config\tankeye.json` 閲岀殑 PLC 鍦板潃閰嶇疆姝ｇ‘銆?

## 6. 绠＄悊鍛樻巿鏉冪爜

姝ｅ紡鍖呬娇鐢?`config\admin_auth.key` 楠岃瘉绠＄悊鍛樻巿鏉冪爜銆傝鏂囦欢涓嶄細鎻愪氦鍒?GitHub锛屼絾濡傛灉瀛樺湪锛屾墦鍖呰剼鏈細澶嶅埗杩涜繍琛屽寘銆?

鏂版満鍣ㄩ娆″垱寤虹鐞嗗憳璐﹀彿鏃讹紝璁╁鏂瑰鍒惰蒋浠舵樉绀虹殑鏈哄櫒鐮侊紝鐒跺悗鐢熸垚 `INIT` 鎺堟潈鐮侊細

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -MachineCode "TK-瀹㈡埛鏈哄櫒鐮? -Purpose INIT
```

蹇樿绠＄悊鍛樺瘑鐮佹椂锛岀敤鍚屼竴鏈哄櫒鐮佺敓鎴?`RESET` 閲嶇疆鐮侊細

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -MachineCode "TK-瀹㈡埛鏈哄櫒鐮? -Purpose RESET
```

濡傛灉鑴氭湰鎻愮ず浣跨敤寮€鍙戦粯璁ゅ瘑閽ワ紝璇存槑娌℃湁鎵惧埌 `config\admin_auth.key`锛屾寮忔墦鍖呭墠闇€瑕佸厛琛ラ綈瀵嗛挜鏂囦欢銆?

## 7. 鎵撳寘杩愯鍖?

鎵撳寘鍛戒护锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4.2 -Force
```

鑴氭湰浼氳嚜鍔ㄤ粠涓嬮潰杩欎簺浣嶇疆鏌ユ壘涓荤▼搴忥細

```text
build\tankeye-openvino_qt_app.exe
build\Release\tankeye-openvino_qt_app.exe
build\RelWithDebInfo\tankeye-openvino_qt_app.exe
build\MinSizeRel\tankeye-openvino_qt_app.exe
```

褰撳墠椤圭洰涓€鑸細鍛戒腑锛?

```text
build\Release\tankeye-openvino_qt_app.exe
```

## 8. 鎵撳寘缁撴灉

鎴愬姛鍚庣敓鎴愶細

```text
dist\TankEye-Iris_1.4.2
dist\TankEye-Iris_1.4.2.zip
```

`dist\TankEye-Iris_1.4.2` 鏄彲鐩存帴杩愯鐨勬枃浠跺す锛宍dist\TankEye-Iris_1.4.2.zip` 鏄粰鏂扮數鑴戞嫹璐濈敤鐨勫帇缂╁寘銆?

## 9. 楠岃瘉鎵撳寘缁撴灉

妫€鏌ュ叧閿枃浠讹細

```powershell
Test-Path .\dist\TankEye-Iris_1.4.2\tankeye-openvino_qt_app.exe
Test-Path .\dist\TankEye-Iris_1.4.2\platforms\qwindows.dll
Test-Path .\dist\TankEye-Iris_1.4.2\models\weights\best_obb.xml
Test-Path .\dist\TankEye-Iris_1.4.2\models\weights\best_seg.xml
Test-Path .\dist\TankEye-Iris_1.4.2\openvino_intel_cpu_plugin.dll
Test-Path .\dist\TankEye-Iris_1.4.2\openvino_intel_gpu_plugin.dll
Test-Path .\dist\TankEye-Iris_1.4.2\config\admin_auth.key
Test-Path .\dist\TankEye-Iris_1.4.2\USAGE_GUIDE.txt
Test-Path .\dist\TankEye-Iris_1.4.2\docs
Test-Path .\dist\TankEye-Iris_1.4.2\AGENTS.md
```

鍓?8 椤瑰簲杩斿洖 `True`锛沗docs` 鍜?`AGENTS.md` 涓ら」蹇呴』杩斿洖 `False`锛岃鏄庤繍琛屽寘鏈寘鍚簮鐮佹枃妗ｅ拰鍗忎綔瑙勫垯鏂囦欢銆?

鏍￠獙鎵撳寘鍑虹殑 exe 鏄惁灏辨槸鏈 build 鐨?exe锛?

```powershell
(Get-FileHash .\build\Release\tankeye-openvino_qt_app.exe).Hash -eq (Get-FileHash .\dist\TankEye-Iris_1.4.2\tankeye-openvino_qt_app.exe).Hash
```

杩斿洖 `True` 琛ㄧず涓€鑷淬€?

## 10. 鍚姩鎵撳寘鍚庣殑绋嬪簭

杩涘叆杩愯鍖呯洰褰曪細

```powershell
cd .\dist\TankEye-Iris_1.4.2
```

姝ｅ父鍚姩锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1
```

妯℃嫙 PLC 鍚姩锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU -SimulatePlc
```

涔熷彲浠ュ弻鍑伙細

```text
launch_tankeye_main_only.vbs
```

## 11. 褰撳墠 1.4.2 琛屼负璇存槑

- 涓荤晫闈㈡瘮渚嬩负宸︿晶鍥惧儚鍖虹害 75%銆佸彸渚ф帶鍒舵爮绾?25%銆?
- 鍙充晶鏍忛噰鐢ㄥ弻鍒楀竷灞€锛屽苟闅忕獥鍙ｅ昂瀵歌嚜閫傚簲銆?
- 鍥惧儚瀹屾暣鏄剧ず锛屽厑璁歌竟缂樼暀鐧斤紝涓嶄娇鐢ㄥ眳涓鍓€?
- 鈥滃姞杞藉浘鐗団€濅娇鐢ㄥ悗鍙扮嚎绋嬭鍙栥€?
- 褰撳墠鐩爣 X/Y 鏈夋満姊板潗鏍囨椂浼樺厛鏄剧ず鏈烘鍧愭爣锛汸LC 鍐欏叆浠嶄娇鐢ㄦ満姊板潗鏍囥€?
- 鐪熷疄澶圭埅妗嗙敱宸ョ▼璁剧疆涓殑澶圭埅闀垮害/瀹藉害鍜屼節鐐规爣瀹氭崲绠楀緱鍒帮紝璐熻矗鍙姄/涓嶅彲鎶撶姸鎬佹樉绀恒€佺鎾炴嫆鎶撱€佹姄鍙栧皠绾?C 鐐瑰拰涓績鍋忕Щ銆?
- 鏃?3 鍊嶅欢闀?OBB 妗嗗凡浠庤繍琛岄€昏緫鍜岀敾闈㈡樉绀轰腑绉婚櫎銆?
- 杩愯鏃ュ織鍏ㄩ儴甯︽椂闂存埑銆?
- 鐣岄潰鍐呪€滆繍琛屾棩蹇椻€濇敮鎸佹渶鏂版棩蹇椼€佽嚜鍔ㄥ埛鏂般€佹悳绱€佺骇鍒繃婊ゃ€佹椂闂磋繃婊ゅ拰鍒嗛〉銆?
- 鏅€氭ā寮忓彧鏄剧ず鐩爣鍒楄〃锛涚鐞嗗憳妯″紡鐧诲綍鍚庢樉绀洪殣钘?鍏ㄦ樉銆佸伐绋嬭缃€佽繍琛屾棩蹇楃瓑璋冭瘯鍏ュ彛銆?
- 杩愯鍖呬腑鐨?`create_desktop_shortcut.ps1` 鐢熸垚妗岄潰鍥炬爣鏃堕粯璁ゅ垱寤哄綋鍓嶇敤鎴峰紑鏈鸿嚜鍚叆鍙ｏ紝榛樿寤惰繜 0 绉掞紱鑷惎鍏ュ彛缁熶竴浣跨敤 `-StartupProfile AutoStart`锛屼細鑷姩鍔犺浇妯″瀷骞跺湪妯″瀷鍔犺浇瀹屾垚鍚庤姹傝繘鍏?PLC 鎶撳彇鑱斿姩锛屽伐绋嬭缃彲鍏抽棴鑷惎骞惰缃惎鍔ㄥ欢杩熺鏁般€?
- 棣栨鍒涘缓绠＄悊鍛橀渶瑕?`INIT` 鎺堟潈鐮侊紱蹇樿瀵嗙爜閲嶇疆闇€瑕?`RESET` 閲嶇疆鐮併€?
- OpenVINO 缂撳瓨鐩綍榛樿涓鸿繍琛屽寘鍐?`openvino_cache`锛屾甯稿惎鍔ㄤ笉浼氬垹闄ょ紦瀛樸€?

## 12. 甯歌闂

### 鎶ラ敊锛歈t app executable not found

鍏堟鏌?exe 鏄惁瀛樺湪锛?

```powershell
Test-Path .\build\Release\tankeye-openvino_qt_app.exe
```

濡傛灉杩斿洖 `False`锛岃鏄庤繕娌＄紪璇戜富绋嬪簭锛屽厛鎵ц锛?

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
```

濡傛灉杩斿洖 `True` 浣嗘墦鍖呬粛鎶ラ敊锛岀‘璁や綘杩愯鐨勬槸鏂扮増鑴氭湰锛?

```powershell
Select-String -Path .\scripts\package_runtime.ps1 -Pattern "Build output"
```

鑳芥悳鍒?`[Package] Build output` 灏辨槸鏂扮増鑴氭湰銆?

### 鎶ラ敊锛氱姝㈣繍琛岃剼鏈?

涓嶈鐩存帴鎵ц锛?

```powershell
.\scripts\package_runtime.ps1
```

鏀圭敤锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4.2 -Force
```

### 鎻愮ず锛歏CINSTALLDIR is not set

杩欐槸 `windeployqt` 鐨勮鍛婏紝涓嶄竴瀹氭槸澶辫触銆傚彧瑕佹渶鍚庡嚭鐜帮細

```text
[Package] Release directory: ...
[Package] Release zip: ...
```

骞朵笖 `dist\TankEye-Iris_1.4.2.zip` 宸茬敓鎴愶紝灏辫鏄庢墦鍖呮垚鍔熴€?

### 鍥剧墖涓枃璺緞瀵艰嚧鍔犺浇澶辫触鎴栧紓甯?

Windows 涓?OpenCV 鐩存帴璇诲彇涓枃璺緞鍙兘涓嶇ǔ瀹氥€備复鏃惰閬挎柟娉曟槸鎶婃祴璇曞浘鐗囨斁鍒扮函鑻辨枃璺緞锛屽苟鎶婂浘鐗囨枃浠跺悕鏀规垚鑻辨枃鎴栨暟瀛椼€?

### 绠＄悊鍛樻巿鏉冪爜鏃犳晥

纭鏈哄櫒鐮佹槸浠庤蒋浠堕噷澶嶅埗鐨勫畬鏁存満鍣ㄧ爜锛涢娆″垱寤轰娇鐢?`-Purpose INIT`锛屽繕璁板瘑鐮侀噸缃娇鐢?`-Purpose RESET`锛涚敓鎴愮爜鐨勭數鑴戝拰鎵撳寘杩愯鍖呬娇鐢ㄥ悓涓€涓?`config\admin_auth.key`銆?

### 鎵撳寘鍚庡姞杞芥ā鍨嬪彉鎱?

杩愯鍖呯涓€娆″姞杞芥ā鍨嬪彲鑳戒細鎱紝杩欐槸姝ｅ父鐜拌薄銆侽penVINO 绗竴娆″姞杞?GPU/CPU 妯″瀷鏃讹紝浼氬仛妯″瀷缂栬瘧骞剁敓鎴愮紦瀛樸€?

杩愯鍖呬細鎶婄紦瀛樹繚瀛樺湪锛?

```text
dist\TankEye-Iris_1.4.2\openvino_cache
```

鍙涓嶅垹闄よ繖涓洰褰曪紝绗簩娆″惎鍔ㄣ€佺浜屾鍔犺浇鍚屼竴濂楁ā鍨嬶紝閫氬父浼氭瘮绗竴娆″揩銆?

濡傛灉鐜板満纭疄闇€瑕佹墜鍔ㄦ竻鐞?OpenVINO 缂撳瓨锛屽彲浠ユ樉寮忓姞鍙傛暟锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -ClearOpenVinoCache
```

姝ｅ父浣跨敤涓嶈鍔犺繖涓弬鏁般€?

### 棣栨鍔犺浇鍥剧墖浠嶇劧鎰熻鍗?

鍥剧墖璇诲彇宸茬粡鏀逛负鍚庡彴绾跨▼锛涘鏋滃垰鍚姩绋嬪簭灏卞姞杞藉浘鐗囷紝鍚庡彴 OpenVINO 妯″瀷缂栬瘧鍙兘姝ｅ湪鍗犵敤 CPU/GPU 璧勬簮锛屽鑷撮娆″浘鐗囨樉绀恒€佺缉鏀惧拰娓叉煋浣撴劅鍙樻參銆傛ā鍨嬬紪璇戝畬鎴愭垨缂撳瓨鍛戒腑鍚庝細鏄庢樉濂借浆銆?

### 鎵撳寘鍚?Device AUTO 鐨勮涓?

褰撳墠杩愯鍖呬細鎶?`-Device AUTO` 鍘熸牱浼犵粰绋嬪簭銆?

甯哥敤鍚姩锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device AUTO
```

寮哄埗 CPU锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device CPU
```

寮哄埗 GPU锛?

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\launch_tankeye.ps1 -Device GPU
```

濡傛灉鎬€鐤戞ā鍨嬪姞杞芥參锛屼紭鍏堟煡鐪嬫渶鏂版棩蹇楅噷鐨勮繖浜涘瓧娈碉細

```text
[OpenVINO] Cache dir:
[OpenVINO] Requested device:
[OpenVINO] Selected device:
[OpenVINO] Compile model ms:
```

鍏朵腑 `Compile model ms` 濡傛灉绗竴娆″緢澶с€佺浜屾鏄庢樉鍙樺皬锛岃鏄庣紦瀛樻鍦ㄦ甯哥敓鏁堛€?

## 13. 鏈€甯哥敤鐨勪竴濂楀懡浠?

鏃ュ父淇敼浠ｇ爜鍚庯紝鐩存帴鎸夐『搴忔墽琛岋細

```powershell
cd D:\work_floder\jiezhifa\TankEye_source_for_new_pc
cmake --build build --config Release --target tankeye-openvino_qt_app tankeye-admin-auth-code tankeye-openvino_frame_postprocess_smoke
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release -Filter "*frame_postprocess_smoke*.exe"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_1.4.2 -Force
```
