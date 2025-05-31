// pch.h: プリコンパイル済みヘッダー ファイルです。
// 次のファイルは、その後のビルドのビルド パフォーマンスを向上させるため 1 回だけコンパイルされます。
// コード補完や多くのコード参照機能などの IntelliSense パフォーマンスにも影響します。
// ただし、ここに一覧表示されているファイルは、ビルド間でいずれかが更新されると、すべてが再コンパイルされます。
// 頻繁に更新するファイルをここに追加しないでください。追加すると、パフォーマンス上の利点がなくなります。

#ifndef PCH_H
#define PCH_H

// プリコンパイルするヘッダーをここに追加します
#include <winsock2.h>
#include "framework.h"
#include <guiddef.h>
#include <computecore.h>
#include <combaseapi.h>
#include <ws2tcpip.h>
#include <hvsocket.h>
#include <mswsock.h>
#include <shlwapi.h>

#include <string>

#include "simdjson/simdjson.h"


// Service ID (0000c544-facb-11e6-bd58-64006a7986d3) (port number: 50050 (0xc544))
constexpr CLSID HVCP_SERVICE_ID = {
    0xc544, 0xfacb, 0x11e6,
    { 0xbd, 0x58, 0x64, 0x00, 0x6a, 0x79, 0x86, 0xd3 }
};

#define SERVICE_PORT 50050

#endif //PCH_H
