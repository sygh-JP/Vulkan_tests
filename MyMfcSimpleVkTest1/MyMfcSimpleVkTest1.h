
// MyMfcSimpleVkTest1.h : PROJECT_NAME アプリケーションのメイン ヘッダー ファイルです。
//

#pragma once

#ifndef __AFXWIN_H__
	#error "PCH に対してこのファイルをインクルードする前に 'stdafx.h' をインクルードしてください"
#endif

#include "resource.h"		// メイン シンボル


// CMyMfcSimpleVkTest1App:
// このクラスの実装については、MyMfcSimpleVkTest1.cpp を参照してください。
//

class CMyMfcSimpleVkTest1App : public CWinApp
{
public:
	CMyMfcSimpleVkTest1App();

// オーバーライド
public:
	virtual BOOL InitInstance();

// 実装

	DECLARE_MESSAGE_MAP()
};

extern CMyMfcSimpleVkTest1App theApp;