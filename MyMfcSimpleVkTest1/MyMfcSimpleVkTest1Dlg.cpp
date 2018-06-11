
// MyMfcSimpleVkTest1Dlg.cpp : 実装ファイル
//

#include "stdafx.h"
#include "MyMfcSimpleVkTest1.h"
#include "MyMfcSimpleVkTest1Dlg.h"
#include <afxdialogex.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// アプリケーションのバージョン情報に使われる CAboutDlg ダイアログ

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// ダイアログ データ
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override; // DDX/DDV サポート

// 実装
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CMyMfcSimpleVkTest1Dlg ダイアログ



CMyMfcSimpleVkTest1Dlg::CMyMfcSimpleVkTest1Dlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_MYMFCSIMPLEVKTEST1_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_vkManager = std::make_unique<MyVkManager>();
}

void CMyMfcSimpleVkTest1Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_PICT1, m_ddxcStaticPict1);
	DDX_Control(pDX, IDC_CHECK_DRAW_BLEND_OBJECTS, m_ddxcCheckDrawBlendingObjects);
}

BEGIN_MESSAGE_MAP(CMyMfcSimpleVkTest1Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CMyMfcSimpleVkTest1Dlg メッセージ ハンドラー

BOOL CMyMfcSimpleVkTest1Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// "バージョン情報..." メニューをシステム メニューに追加します。

	// IDM_ABOUTBOX は、システム コマンドの範囲内になければなりません。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// このダイアログのアイコンを設定します。アプリケーションのメイン ウィンドウがダイアログでない場合、
	//  Framework は、この設定を自動的に行います。
	SetIcon(m_hIcon, TRUE);			// 大きいアイコンの設定
	SetIcon(m_hIcon, FALSE);		// 小さいアイコンの設定

	// TODO: 初期化をここに追加します。

	// リサイズ処理を書くのが面倒なので、とりあえず固定サイズのコントロールに Vulkan 描画する。
	m_ddxcStaticPict1.SetWindowPos(nullptr, 0, 0, 640, 480, SWP_NOZORDER | SWP_NOMOVE);

	HWND hWnd = m_ddxcStaticPict1.GetSafeHwnd();
	CRect clientRect;
	m_ddxcStaticPict1.GetClientRect(clientRect);
	const UINT width = clientRect.Width();
	const UINT height = clientRect.Height();
	try
	{
		_ASSERTE(m_vkManager);
		m_vkManager->Init(hWnd, width, height);
	}
	catch (const std::exception& ex)
	{
		const CString strTypeName(typeid(ex).name());
		const CString strErr(ex.what());
		ATLTRACE(_T("Exception=%s, Message=\"%s\"\n"), strTypeName.GetString(), strErr.GetString());
		AfxMessageBox(strErr);
		//this->OnCancel();
		this->EndDialog(IDCANCEL);
		return TRUE;
	}

	m_ddxcCheckDrawBlendingObjects.SetCheck(m_vkManager->GetDrawsBlendingObjects() ? BST_CHECKED : BST_UNCHECKED);

	m_updateTimerId = this->SetTimer(ExpectedUpdateTimerId, ExpectedUpdateTimerPeriodMs, nullptr);

	return TRUE;  // フォーカスをコントロールに設定した場合を除き、TRUE を返します。
}

void CMyMfcSimpleVkTest1Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// ダイアログに最小化ボタンを追加する場合、アイコンを描画するための
//  下のコードが必要です。ドキュメント/ビュー モデルを使う MFC アプリケーションの場合、
//  これは、Framework によって自動的に設定されます。

void CMyMfcSimpleVkTest1Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 描画のデバイス コンテキスト

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// クライアントの四角形領域内の中央
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// アイコンの描画
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();

		if (m_vkManager)
		{
			try
			{
				m_vkManager->Render();
			}
			catch (const std::exception& ex)
			{
				const CString strTypeName(typeid(ex).name());
				const CString strErr(ex.what());
				ATLTRACE(_T("Exception=%s, Message=\"%s\"\n"), strTypeName.GetString(), strErr.GetString());
				// HACK: ポーリング タイマーおよびレンダリングを停止してエラーメッセージ ダイアログを表示し、終了するべき。
			}
		}
	}
}

// ユーザーが最小化したウィンドウをドラッグしているときに表示するカーソルを取得するために、
//  システムがこの関数を呼び出します。
HCURSOR CMyMfcSimpleVkTest1Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CMyMfcSimpleVkTest1Dlg::OnDestroy()
{
	// ウィンドウを破棄する前に GPU リソースおよび Vulkan を解放する。

	if (m_updateTimerId)
	{
		this->KillTimer(m_updateTimerId);
		m_updateTimerId = 0;
	}

	m_vkManager.reset();

	CDialogEx::OnDestroy();

	// TODO: ここにメッセージ ハンドラー コードを追加します。
}

void CMyMfcSimpleVkTest1Dlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: ここにメッセージ ハンドラー コードを追加するか、既定の処理を呼び出します。

	if (nIDEvent == m_updateTimerId && m_vkManager)
	{
		auto vRotAngle = m_vkManager->GetRotAngle();
		vRotAngle.z += DirectX::XMConvertToRadians(0.2f);
		if (vRotAngle.z >= DirectX::XM_2PI)
		{
			vRotAngle.z = 0;
		}
		m_vkManager->SetRotAngle(vRotAngle);
		m_vkManager->SetDrawsBlendingObjects(m_ddxcCheckDrawBlendingObjects.GetCheck() != BST_UNCHECKED);

#if 0
		this->Invalidate(false);
#else
		// Direct3D ではターゲットとなるピクチャコントロールの背景の消去有無にかかわらずちらつきが発生しないが、
		// Vulkan の場合は背景を消去するとちらつく。
		// 回避方法のひとつとして、親ダイアログに WS_CLIPCHILDREN を指定する方法があるが、グループボックスと共存できないので却下。
		CRect pictureRect;
		m_ddxcStaticPict1.GetWindowRect(pictureRect);
		this->ScreenToClient(pictureRect);
		this->InvalidateRect(pictureRect, false);
#endif
	}

	CDialogEx::OnTimer(nIDEvent);
}
