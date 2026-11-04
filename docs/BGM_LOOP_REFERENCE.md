# BGM 原作循环与播放

菜单、六面道中/Boss、Extra 三槽、结局及音乐室统一使用 `audio::BgmPlayer`。首次播放从曲首开始，之后重复 `[loopStart, loopEnd)`；前奏只播放一次，循环终点以后的尾部不播放。素材目录与关卡清单使用 FLAC，旁路 `.loop` 保存采样率和整数循环帧。

## 原作依据与单位

来源为本机新典 `th06nc.exe`，SHA-256 为 `48630a42a2eb6762d0db2a7e0d151203efbe67220fef7d2f9d0d51857928ac82`。`th06MD.dat` 中的 `th06_NN.pos` 对应 `data/bgm/th06_NN.opus`；带 `o` 后缀的 `.pos` 对应另一套 `bgm2` 音源，不能混用。本次保持项目既有 `bgm` 音源，导入目录内 17 首曲目；未使用的第 18 首不随包增加。

`.pos` 恰好包含两个小端整数，分别为 44.1 kHz 的循环起点和绝对终点，单位是每声道同一时刻组成的采样帧，不是交错样本数或字节数。原生 RVA `0x7daa0..0x7daef` 对两个字段分别执行 `value * 480 / 441`，正数向下取整；`0xcb5d0` 保存起止值，`0xc8f38..0xc8f50` 分别交给起点/终点设置器。音频读取器以终点限制读取长度，回绕时恢复起点，因此终点采用排他边界。

源 Nintendo Opus 为 48 kHz 双声道，头部 `pre-skip=120`。原生读取器在 RVA `0x80d88..0x80dee` 丢弃 pre-skip，seek 在 `0x80e6c` 起把逻辑采样位置加回 skip 并预热解码器。离线重封装保留 OpusHead 的 skip，ffmpeg 解码时消费一次；循环坐标不再次扣除 120。

`th06_07.pos` 的换算终点为 4955067，但音频去除 pre-skip 后只有 4954440 帧。原生 RVA `0x264e99` 返回实际短读，`0xcb196` 检测 EOF 后立即回环，`0xcf191..0xcf239` 恢复循环起点并 seek，不补足终点差值。导入器据此使用 `min(配置终点, PCM总帧数)`；本曲实际区间为 `[1008000,4954440)`，报告同时保留配置终点与有效终点。

完整素材哈希、原始 `.pos` 字节、转换结果及 PCM 大小记录在 `assets/source/music/native_reference.json`。运行时不读取 EXE、MD、原始 Opus 或本机安装路径。

## 播放器与生命周期

FLAC 在加载时预解码为 16 bit 交错 PCM。音频回调在同一个输出缓冲中完成终点到起点的拼接，连续请求可跨越多个循环；不读盘、不分配、不使用自有锁，不依赖 `Game::Update` 或 `UpdateAudio` 补样。raylib 负责输出设备的采样率转换。

播放器独占 PCM 与 AudioStream，可移动但不可复制。raylib 回调没有 userdata，因此最多同时注册 8 个固定回调槽，移动时状态地址不变。空槽不足或素材/元数据无效时加载失败并保留旧播放器。卸载先停流，再通过 raylib 音频锁同步移除回调，随后释放 PCM 和槽；音频设备须在全部播放器销毁后关闭。

主线程通过原子请求交付 seek，音频回调在下一次取样时应用；位置查询返回已交付/待交付的源位置，不能视作扬声器精确时钟。暂停保留位置，恢复不重播；停止与重新播放复位至曲首。播放超出首次终点后的定位为 `loopStart + (frame-loopStart) % (loopEnd-loopStart)`，回放恢复正常速度时也使用此规则。

预解码内存：当前最长单曲 PCM 约 32.3 MiB；标题曲与 Extra 的三首音乐同时持有约 107.9 MiB，不包含流缓冲和图形资源。17 首 FLAC 合计约 245.5 MiB，音乐室按需只保留一首，切换时临时加载下一首。

## 重新导入

先用已核对的只读工具解出 `th06MD.dat` 的 `th06_01.pos` 至 `th06_17.pos`，再运行：

```bash
python3 tools/import_bgm.py --native-root "$GAME" --positions "$POSITIONS" \
  --runtime-root assets/runtime --report assets/source/music/native_reference.json
```

导入器核对 EXE 身份、原始容器、采样率、声道、解码帧数和循环范围，全部曲目验证完成后才发布 FLAC 和旁路文件。FLAC 是原 Opus 解码 PCM 的无损存储，不恢复 Opus 编码前的信息，也不再经历 MP3 有损压缩。

## 验证入口

`koumakyou_bgm_player_tests` 无需音频设备，精确比较手写 PCM 序列，覆盖前奏、边界、多次循环、交错声道与定位。`tests/test_import_bgm.py` 检查离线导入协议、整数换算及容器校验。

`koumakyou_music_tests` 使用真实音频设备且强制静音，逐曲检查 17 首 FLAC 的实际回调、主线程不补样、暂停、循环跨界、多圈定位及反复卸载。`koumakyou_lifecycle_tests --dialogue-music-only` 和 `--extra-assets-only` 复验对白切曲顺序、同槽不重启、停止后重播、同步/异步关卡加载及素材释放。设备计时允许输出周期差异，采样接缝在无设备测试中精确比较，不能通过放宽 PCM 比较掩盖漏样或重复样本。

可复现命令（图形测试期间不要构建会重建素材目录的目标）：

```bash
cmake --build build-debug --target koumakyou koumakyou_tests koumakyou_bgm_player_tests koumakyou_music_tests koumakyou_lifecycle_tests -j 6
ctest --test-dir build-debug --output-on-failure
./build-debug/koumakyou_music_tests /tmp/bgm-loop-seams
./build-debug/koumakyou_lifecycle_tests /tmp/bgm-dialogue --dialogue-music-only
./build-debug/koumakyou_lifecycle_tests /tmp/bgm-extra --extra-assets-only
```

音乐设备测试可选参数导出各曲循环点前后各一秒的 WAV（1～17 对应音乐室目录），供人工复听；自动检查逐样本核对输出，不能代替听感验收。

本次验证：应用、核心测试、BGM 设备测试、关卡生命周期和设置界面目标完整构建成功；全部 10 项 CTest 通过（9 项通用测试与 1 项参考工具测试分批运行），其中核心回归约 651 秒。真实设备 17 曲的逐样本接缝、循环跨界、回放定位、暂停与反复卸载全部通过；对白三槽演出及 Extra 正式素材同步/异步加载两项图形/音频专项通过。人工复听尚未作为自动验收的一部分。
