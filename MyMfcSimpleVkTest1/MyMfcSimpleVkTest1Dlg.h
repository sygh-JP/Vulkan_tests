
// MyMfcSimpleVkTest1Dlg.h : ヘッダー ファイル
//

#pragma once

#include <afxwin.h>
#include <afxdialogex.h>

#include "MyVkManager.h"

// CMyMfcSimpleVkTest1Dlg ダイアログ
class CMyMfcSimpleVkTest1Dlg : public CDialogEx
{
	// コンストラクション
public:
	explicit CMyMfcSimpleVkTest1Dlg(CWnd* pParent = nullptr); // 標準コンストラクター

	// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MYMFCSIMPLEVKTEST1_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override; // DDX/DDV サポート

// 実装
protected:
	HICON m_hIcon;

	// 生成された、メッセージ割り当て関数
	virtual BOOL OnInitDialog() override;
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
private:
	CStatic m_ddxcStaticPict1;
	CButton m_ddxcCheckDrawBlendingObjects;
private:
	std::unique_ptr<MyVkManager> m_vkManager;
	UINT_PTR m_updateTimerId = 0;
	static const UINT_PTR ExpectedUpdateTimerId = 1001;
	static const DWORD ExpectedUpdateTimerPeriodMs = 20;
public:
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
