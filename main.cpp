#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlwapi.h>

#include <atlbase.h>
#include <atlcom.h>
#undef SubclassWindow
#include <atlwin.h>
#include <atltypes.h>
#include <atlimage.h>

#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <math.h>

#include <vector>
#include <string>
#include <algorithm>

#include "resource.h"

using namespace ATL;

//////////////////////////////////////////////////////////////////////////////////////////////////

class CPicoDataEditModule : public ATL::CAtlExeModuleT<CPicoDataEditModule>
{
};
CPicoDataEditModule g_app;

//////////////////////////////////////////////////////////////////////////////////////////////////

struct ITEM
{
    std::string filename;
    float LeftEyeX = -1;
    float LeftEyeY = -1;
    float RightEyeX = -1;
    float RightEyeY = -1;
    float NoseX = -1;
    float NoseY = -1;
    float MouthX = -1;
    float MouthY = -1;
    BOOL bDeleted = FALSE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

float GetDlgItemFloat(HWND hwnd, INT nItemID)
{
    char buf[128];
    GetDlgItemTextA(hwnd, nItemID, buf, sizeof(buf));
    return (float)atof(buf);
}

BOOL SetDlgItemFloat(HWND hwnd, INT nItemID, float value)
{
    char buf[128];
    StringCchPrintfA(buf, _countof(buf), "%f", value);
    return SetDlgItemTextA(hwnd, nItemID, buf);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#define MYWM_LBUTTONDOWN (WM_USER + 100)
#define MYWM_MOUSEMOVE (WM_USER + 101)

class CImageView 
    : public CWindowImpl<CImageView>
{
    HBITMAP m_hbm = NULL;
    SIZE m_realSize = { 0, 0 };
    SIZE m_viewSize = { 0, 0 };
    float m_zoom = 1.0;
public:
    POINTF m_Points[4] =
    {
        { -1, -1 },
        { -1, -1 },
        { -1, -1 },
        { -1, -1 },
    };
    COLORREF m_Colors[4] =
    {
        RGB(255, 0, 0),
        RGB(0, 255, 0),
        RGB(255, 0, 255),
        RGB(0, 255, 255),
    };
    BOOL m_bMButton = FALSE;
    POINT m_ptDragging;
    POINT m_ptScroll = { 0, 0 };
public:
    CImageView() 
    {
    }

    ~CImageView()
    {
        ::DeleteObject(m_hbm);
    }

    POINTF ViewToReal(const POINTF& pt) const
    {
        if (m_hbm == NULL || m_viewSize.cx <= 0 || m_viewSize.cy <= 0)
            return { -1, -1 };
        return {
            (pt.x - m_ptScroll.x) * m_realSize.cx / m_viewSize.cx / m_zoom,
            (pt.y - m_ptScroll.y) * m_realSize.cy / m_viewSize.cy / m_zoom,
        };
    }

    POINTF RealToView(const POINTF& pt) const
    {
        if (m_hbm == NULL || m_realSize.cx <= 0 || m_realSize.cy <= 0)
            return { -1, -1 };
        return {
            pt.x * m_viewSize.cx / m_realSize.cx * m_zoom + m_ptScroll.x,
            pt.y * m_viewSize.cy / m_realSize.cy * m_zoom + m_ptScroll.y,
        };
    }

    void doDrawPoint(HDC hDC, POINTF& pt, INT i)
    {
        if (pt.x < 0 || pt.y < 0)
            return;

        POINTF ptView = RealToView(pt);
        POINT pt2 = { LONG(ptView.x), LONG(ptView.y) };
        INT cxy = 5;

        ::SelectObject(hDC, ::GetStockObject(BLACK_PEN));
        HGDIOBJ hbrOld = ::SelectObject(hDC, ::CreateSolidBrush(m_Colors[i]));
        ::Ellipse(hDC, pt2.x - cxy, pt2.y - cxy, pt2.x + cxy, pt2.y + cxy);
        ::DeleteObject(::SelectObject(hDC, hbrOld));

        LPCSTR psz = NULL;
        INT dx = 0, dy = 0;
        switch (i)
        {
        case 0:
            psz = "L.Eye";
            dx = -20;
            dy = -10;
            break;
        case 1:
            psz = "R.Eye";
            dx = +20;
            dy = -10;
            break;
        case 2:
            psz = "Nose";
            dx = +20;
            break;
        case 3:
            psz = "Mouth";
            dy = +15;
            break;
        }
        CRect rc(pt2, pt2);
        rc.InflateRect(100, 100);
        rc.OffsetRect(dx, dy);
        ::SelectObject(hDC, ::GetStockObject(DEFAULT_GUI_FONT));
        ::DrawTextA(hDC, psz, lstrlenA(psz), &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    BEGIN_MSG_MAP(CImageView)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkGnd)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMButtonDown)
        MESSAGE_HANDLER(WM_MBUTTONUP, OnMButtonUp)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
    END_MSG_MAP()

    LRESULT OnMButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        SetCapture();
        m_bMButton = TRUE;
        INT x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        m_ptDragging = { x, y };
        return 0;
    }

    LRESULT OnMButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        m_bMButton = FALSE;
        ReleaseCapture();
        return 0;
    }

    LRESULT OnEraseBkGnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        HDC hDC = (HDC)wParam;
        CRect rc;
        GetClientRect(rc);
        FillRect(hDC, rc, (HBRUSH)GetStockObject(DKGRAY_BRUSH));
        return 0;
    }

    LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        SHORT zDelta = (SHORT)HIWORD(wParam);
        if (zDelta < 0)
            m_zoom *= 0.9f;
        else if (zDelta > 0)
            m_zoom *= 1.1f;
        Invalidate(TRUE);
        return 0;
    }

    LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        INT x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        HWND hwndParent = ::GetParent(m_hWnd);
        ::PostMessage(hwndParent, MYWM_LBUTTONDOWN, 0, lParam);
        return 0;
    }

    LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        INT x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

        if (m_bMButton)
        {
            POINT pt = { x, y };
            m_ptScroll.x -= m_ptDragging.x - pt.x;
            m_ptScroll.y -= m_ptDragging.y - pt.y;
            m_ptDragging = pt;
            Invalidate(TRUE);
            return 0;
        }

        HWND hwndParent = ::GetParent(m_hWnd);
        ::PostMessage(hwndParent, MYWM_MOUSEMOVE, 0, lParam);
        return 0;
    }

    LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        PAINTSTRUCT ps;
        if (HDC hDC = BeginPaint(&ps))
        {
            HDC hdcMem = ::CreateCompatibleDC(hDC);
            HGDIOBJ hbmOld = ::SelectObject(hdcMem, m_hbm);
            ::SetStretchBltMode(hdcMem, HALFTONE);

            CRect rc;
            GetClientRect(rc);
            ::IntersectClipRect(hDC, rc.left, rc.top, rc.right, rc.bottom);

            ::StretchBlt(hDC, m_ptScroll.x, m_ptScroll.y, 
                         LONG(m_viewSize.cx * m_zoom), 
                         LONG(m_viewSize.cy * m_zoom),
                         hdcMem, 0, 0, m_realSize.cx, m_realSize.cy,
                         SRCCOPY);
            ::SelectObject(hdcMem, hbmOld);
            ::DeleteDC(hdcMem);

            for (INT i = 0; i < 4; ++i)
                doDrawPoint(hDC, m_Points[i], i);
            EndPaint(&ps);
        }
        return 0;
    }

    void doSetBitmap(HBITMAP hbm)
    {
        if (m_hbm)
        {
            ::DeleteObject(m_hbm);
            m_hbm = NULL;
        }

        BITMAP bm;
        if (!::GetObject(hbm, sizeof(BITMAP), &bm))
        {
            Invalidate();
            return;
        }

        m_hbm = hbm;
        m_realSize.cx = bm.bmWidth;
        m_realSize.cy = bm.bmHeight;

        CRect rc;
        GetClientRect(&rc);

        SIZE size = m_realSize;
        INT cx = rc.Width(), cy = rc.Height();
        while (size.cx > cx || size.cy > cy)
        {
            size.cx = (size.cx * 4) / 5;
            size.cy = (size.cy * 4) / 5;
        }
        m_viewSize = size;

        m_bMButton = FALSE;
        m_ptScroll = { 0, 0 };

        Invalidate();
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class CMainWindow : public CDialogImpl<CMainWindow, CWindow>
{
public:
    enum { IDD = IDD_MAIN };
    CMainWindow();
    virtual ~CMainWindow();
    std::vector<ITEM> m_items;
    BOOL m_bItemIsModified = FALSE;
    CStringW m_strPath;
    CStringW m_strTxtFile;

    VOID ResetSettings();
    BOOL LoadSettings();
    BOOL SaveSettings();

    BEGIN_MSG_MAP(CMainWindow)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_DROPFILES, OnDropFiles)
        MESSAGE_HANDLER(MYWM_MOUSEMOVE, OnImageMouseMove)
        MESSAGE_HANDLER(MYWM_LBUTTONDOWN, OnImageLButtonDown)
    END_MSG_MAP()

    CImageView m_imageView;

    LRESULT OnImageMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        POINTF pt = { (float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam) };
        pt = m_imageView.ViewToReal(pt);
        WCHAR szText[128];
        StringCchPrintfW(szText, _countof(szText), L"%ld, %ld", (LONG)pt.x, (LONG)pt.y);
        ::SetDlgItemTextW(m_hWnd, stc11, szText);
        return 0;
    }

    LRESULT OnImageLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        POINTF pt = { (float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam) };
        pt = m_imageView.ViewToReal(pt);
        if (IsDlgButtonChecked(rad1) == BST_CHECKED)
        {
            SetDlgItemInt(edt1, (LONG)pt.x, TRUE);
            SetDlgItemInt(edt2, (LONG)pt.y, TRUE);
            CheckRadioButton(rad1, rad4, rad2);
            m_imageView.m_Points[0] = pt;
            m_imageView.Invalidate();
            m_bItemIsModified = TRUE;
        }
        else if (IsDlgButtonChecked(rad2) == BST_CHECKED)
        {
            SetDlgItemInt(edt3, (LONG)pt.x, TRUE);
            SetDlgItemInt(edt4, (LONG)pt.y, TRUE);
            CheckRadioButton(rad1, rad4, rad3);
            m_imageView.m_Points[1] = pt;
            m_imageView.Invalidate();
            m_bItemIsModified = TRUE;
        }
        else if (IsDlgButtonChecked(rad3) == BST_CHECKED)
        {
            SetDlgItemInt(edt5, (LONG)pt.x, TRUE);
            SetDlgItemInt(edt6, (LONG)pt.y, TRUE);
            CheckRadioButton(rad1, rad4, rad4);
            m_imageView.m_Points[2] = pt;
            m_imageView.Invalidate();
            m_bItemIsModified = TRUE;
        }
        else if (IsDlgButtonChecked(rad4) == BST_CHECKED)
        {
            SetDlgItemInt(edt7, (LONG)pt.x, TRUE);
            SetDlgItemInt(edt8, (LONG)pt.y, TRUE);
            CheckRadioButton(rad1, rad4, rad1);
            m_imageView.m_Points[3] = pt;
            m_imageView.Invalidate();
            m_bItemIsModified = TRUE;
        }
        return 0;
    }

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        CheckRadioButton(rad1, rad4, rad1);

        SetDlgItemInt(edt1, -1, TRUE);
        SetDlgItemInt(edt2, -1, TRUE);
        SetDlgItemInt(edt3, -1, TRUE);
        SetDlgItemInt(edt4, -1, TRUE);
        SetDlgItemInt(edt5, -1, TRUE);
        SetDlgItemInt(edt6, -1, TRUE);
        SetDlgItemInt(edt7, -1, TRUE);
        SetDlgItemInt(edt8, -1, TRUE);

        m_imageView.SubclassWindow(::GetDlgItem(m_hWnd, stc9));

        // Set Icon
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON));
        HICON hIconSm = (HICON)LoadImage(
            GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAINICON), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);

        // Open parameter
        if (__argc > 1)
            doLoadTxtFile(__targv[1]);

        return 0;
    }

    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        return 0;
    }

    BOOL doLoadTxtFile(LPCTSTR pszFile)
    {
        m_items.clear();
        if (FILE *fin = _tfopen(pszFile, "r"))
        {
            char buf[256], filename[MAX_PATH];
            while (fgets(buf, 256, fin))
            {
                StrTrimA(buf, " \t\r\n");
                ITEM item;
                if (sscanf(buf, "%s %f %f %f %f %f %f %f %f",
                           filename,
                           &item.LeftEyeX, &item.LeftEyeY,
                           &item.RightEyeX, &item.RightEyeY,
                           &item.NoseX, &item.NoseY,
                           &item.MouthX, &item.MouthY))
                {
                    buf[sizeof(buf) - 1];
                    item.filename = filename;
                    if (item.filename.size() &&
                        item.LeftEyeX > 0 && item.LeftEyeY > 0 &&
                        item.RightEyeX > 0 &&  item.RightEyeY > 0 &&
                        item.NoseX > 0 && item.NoseY > 0 &&
                        item.MouthX > 0 && item.MouthY > 0)
                    {
                        m_items.push_back(item);
                    }
                }
            }

            fclose(fin);
        }

        m_bItemIsModified = FALSE;
        doUpdateListBox();

        // Save file location info
        CStringW strFile(pszFile);
        INT ich = strFile.ReverseFind(L'\\');
        CStringW strPath = strFile.Left(ich);
        m_strPath = strPath;

        WCHAR szText[MAX_PATH];
        ::GetFullPathNameW(strFile, MAX_PATH, szText, NULL);
        m_strTxtFile = szText;

        return 0;
    }

    void doUpdateListBox()
    {
        HWND hwndListBox = ::GetDlgItem(m_hWnd, lst1);
        ListBox_ResetContent(hwndListBox);

        size_t index = 0;
        for (auto& item: m_items)
        {
            std::string str = "#";
            str += std::to_string(index++);
            str += ": ";
            str += item.filename;
            ListBox_AddString(hwndListBox, str.c_str());
        }

        ListBox_SetCurSel(hwndListBox, 0);
        OnListBoxSelect(0);
    }

    BOOL OnOK()
    {
        // TODO: Do something
        return TRUE;
    }

    LRESULT OnCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (OnOK())
            {
                EndDialog(IDOK);
            }
            break;
        case IDCANCEL:
            EndDialog(IDCANCEL);
            break;
        case lst1: // ListBox
            if (HIWORD(wParam) == LBN_SELCHANGE)
            {
                INT iItem = ListBox_GetCurSel(::GetDlgItem(m_hWnd, lst1));
                if (iItem >= 0)
                {
                    OnListBoxSelect(iItem);
                }
            }
            break;
        case psh1: // Previous Image
            checkModified();
            doGoPrevImage();
            break;
        case psh2: // Previous Face
            checkModified();
            doGoPrevFace();
            break;
        case psh3: // Next Face
            checkModified();
            doGoNextFace();
            break;
        case psh4: // Next Image
            checkModified();
            doGoNextImage();
            break;
        case psh5: // Save
            checkModified();
            if (m_strTxtFile.IsEmpty())
                doSaveAs();
            else
                doSave(m_strTxtFile);
            break;
        case psh6: // Sort
            checkModified();
            std::sort(m_items.begin(), m_items.end(), [&](const ITEM& i1, const ITEM& i2){
                return i1.filename < i2.filename;
            });
            doUpdateListBox();
            break;
        case edt1: // Left Eye X
        case edt2: // Left Eye Y
        case edt3: // Right Eye X
        case edt4: // Right Eye Y
        case edt5: // Nose X
        case edt6: // Nose Y
        case edt7: // Mouth X
        case edt8: // Mouth Y
            m_bItemIsModified = TRUE;
            break;
        case stc1:
        case stc2:
        case stc3:
        case stc4:
        case stc5:
        case stc6:
        case stc7:
        case stc8:
            {
                INT nEditID = LOWORD(wParam) - stc1 + edt1;
                HWND hEdit = ::GetDlgItem(m_hWnd, nEditID);
                Edit_SetSel(hEdit, 0, -1);
                ::SetFocus(hEdit);
            }
            break;
        case stc9: // Image
            if (HIWORD(wParam) == STN_CLICKED)
            {
            }
            break;
        }
        return 0;
    }

    void checkModified()
    {
        if (!m_bItemIsModified)
            return;

        HWND hListBox = ::GetDlgItem(m_hWnd, lst1);
        INT iItem = ListBox_GetCurSel(hListBox);
        INT cItems = ListBox_GetCount(hListBox);
        if (iItem < 0 || iItem >= cItems)
        {
            m_bItemIsModified = FALSE;
            return;
        }

        m_items[iItem].LeftEyeX = ::GetDlgItemFloat(m_hWnd, edt1);
        m_items[iItem].LeftEyeY = ::GetDlgItemFloat(m_hWnd, edt2);
        m_items[iItem].RightEyeX = ::GetDlgItemFloat(m_hWnd, edt3);
        m_items[iItem].RightEyeY = ::GetDlgItemFloat(m_hWnd, edt4);
        m_items[iItem].NoseX = ::GetDlgItemFloat(m_hWnd, edt5);
        m_items[iItem].NoseY = ::GetDlgItemFloat(m_hWnd, edt6);
        m_items[iItem].MouthX = ::GetDlgItemFloat(m_hWnd, edt7);
        m_items[iItem].MouthY = ::GetDlgItemFloat(m_hWnd, edt8);
        m_bItemIsModified = FALSE;
    }

    void OnListBoxSelect(INT iItem)
    {
        if (iItem < 0 || iItem >= (INT)m_items.size())
        {
            ::SetDlgItemTextA(m_hWnd, stc10, NULL);
            ::SetDlgItemTextA(m_hWnd, edt1, NULL);
            ::SetDlgItemTextA(m_hWnd, edt2, NULL);
            ::SetDlgItemTextA(m_hWnd, edt3, NULL);
            ::SetDlgItemTextA(m_hWnd, edt4, NULL);
            ::SetDlgItemTextA(m_hWnd, edt5, NULL);
            ::SetDlgItemTextA(m_hWnd, edt6, NULL);
            ::SetDlgItemTextA(m_hWnd, edt7, NULL);
            ::SetDlgItemTextA(m_hWnd, edt8, NULL);
            m_imageView.doSetBitmap(NULL);
            return;
        }

        std::string str = "#";
        str += std::to_string(iItem);
        ::SetDlgItemTextA(m_hWnd, stc10, str.c_str());

        INT i = 0;
        auto& item = m_items[iItem];
        m_imageView.m_Points[0].x = item.LeftEyeX;
        ::SetDlgItemInt(m_hWnd, edt1, (LONG)item.LeftEyeX, TRUE);
        m_imageView.m_Points[0].y = item.LeftEyeY;
        ::SetDlgItemInt(m_hWnd, edt2, (LONG)item.LeftEyeY, TRUE);
        m_imageView.m_Points[1].x = item.RightEyeX;
        ::SetDlgItemInt(m_hWnd, edt3, (LONG)item.RightEyeX, TRUE);
        m_imageView.m_Points[1].y = item.RightEyeY;
        ::SetDlgItemInt(m_hWnd, edt4, (LONG)item.RightEyeY, TRUE);
        m_imageView.m_Points[2].x = item.NoseX;
        ::SetDlgItemInt(m_hWnd, edt5, (LONG)item.NoseX, TRUE);
        m_imageView.m_Points[2].y = item.NoseY;
        ::SetDlgItemInt(m_hWnd, edt6, (LONG)item.NoseY, TRUE);
        m_imageView.m_Points[3].x = item.MouthX;
        ::SetDlgItemInt(m_hWnd, edt7, (LONG)item.MouthX, TRUE);
        m_imageView.m_Points[3].y = item.MouthY;
        ::SetDlgItemInt(m_hWnd, edt8, (LONG)item.MouthY, TRUE);

        CString filename(m_items[iItem].filename.c_str());
        CImage image;
        if (m_strPath.IsEmpty())
        {
            image.Load(filename);
        }
        else
        {
            CString str(m_strPath);
            str += TEXT('\\');
            str += CString(filename);
            image.Load(str);
        }
        HBITMAP hbm = image.Detach();
        m_imageView.doSetBitmap(hbm);
    }

    void doGoPrevImage()
    {
        HWND hListBox = ::GetDlgItem(m_hWnd, lst1);
        INT iItem = ListBox_GetCurSel(hListBox);
        if (iItem < 0)
            return;

        auto filename = m_items[iItem].filename;
        while (iItem > 0)
        {
            --iItem;

            if (m_items[iItem].filename != filename)
            {
                ListBox_SetCurSel(hListBox, iItem);
                OnListBoxSelect(iItem);
                return;
            }

            filename = m_items[iItem].filename;
        }
    }

    void doGoPrevFace()
    {
        HWND hListBox = ::GetDlgItem(m_hWnd, lst1);
        INT iItem = ListBox_GetCurSel(hListBox);
        if (iItem < 0)
            return;

        ListBox_SetCurSel(hListBox, iItem - 1);
        OnListBoxSelect(iItem - 1);
    }

    void doGoNextImage()
    {
        HWND hListBox = ::GetDlgItem(m_hWnd, lst1);
        INT iItem = ListBox_GetCurSel(hListBox);
        INT cItems = ListBox_GetCount(hListBox);
        if (iItem >= cItems - 1)
            return;

        auto filename = m_items[iItem].filename;
        while (iItem < cItems - 1)
        {
            ++iItem;

            if (m_items[iItem].filename != filename)
            {
                ListBox_SetCurSel(hListBox, iItem);
                OnListBoxSelect(iItem);
                return;
            }
        }
    }

    void doGoNextFace()
    {
        HWND hListBox = ::GetDlgItem(m_hWnd, lst1);
        INT iItem = ListBox_GetCurSel(hListBox);
        INT cItems = ListBox_GetCount(hListBox);
        if (iItem >= cItems - 1)
            return;

        ListBox_SetCurSel(hListBox, iItem + 1);
        OnListBoxSelect(iItem + 1);
    }

    void doLoadImageFile(LPCTSTR szFile)
    {
        ITEM item;
        item.filename = CStringA(PathFindFileName(szFile));
        size_t index = m_items.size();
        m_items.push_back(item);

        CString strPath(m_strPath);
        strPath += CString("\\");
        strPath += CString(PathFindFileName(szFile));
        ::CopyFile(szFile, strPath, FALSE);

        HWND hwndListBox = ::GetDlgItem(m_hWnd, lst1);
        std::string str = "#";
        str += std::to_string(index);
        str += ": ";
        str += item.filename;
        INT iItem = ListBox_AddString(hwndListBox, str.c_str());
        ListBox_SetCurSel(hwndListBox, iItem);
        OnListBoxSelect(iItem);
    }

    LRESULT OnDropFiles(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
    {
        HDROP hDrop = (HDROP)wParam;
        TCHAR szFile[MAX_PATH];
        INT iItem, cItems = ::DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
        for (iItem = 0; iItem < cItems; ++iItem)
        {
            ::DragQueryFile(hDrop, iItem, szFile, _countof(szFile));
            CString pchDotExt = PathFindExtension(szFile);
            if (pchDotExt.CompareNoCase(TEXT(".txt")) == 0)
            {
                doLoadTxtFile(szFile);
                break;
            }
            if (pchDotExt.CompareNoCase(TEXT(".jpg")) == 0 ||
                pchDotExt.CompareNoCase(TEXT(".jpeg")) == 0 ||
                pchDotExt.CompareNoCase(TEXT(".png")) == 0)
            {
                doLoadImageFile(szFile);
            }
        }
        ::DragFinish(hDrop);
        return 0;
    }

    void doSaveAs()
    {
        WCHAR szFile[MAX_PATH] = L"";
        OPENFILENAMEW ofn = { sizeof(ofn), m_hWnd };
        ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0";
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = _countof(szFile);
        ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING;
        ofn.lpstrDefExt = L"txt";
        if (::GetSaveFileNameW(&ofn))
        {
            if (!doSave(szFile))
            {
                MessageBox(TEXT("ERROR!"), NULL, MB_ICONERROR);
            }
        }
    }

    float add_random(float value)
    {
        float floor_value = (float)floor(value);
        return float(floor_value + (rand() / (double)(RAND_MAX - 1)));
    }

    BOOL doSave(LPCWSTR pszFile)
    {
        if (FILE *fout = _wfopen(pszFile, L"w"))
        {
            for (auto& item: m_items)
            {
                fprintf(fout,
                    "%s %e %e %e %e %e %e %e %e \n",
                    item.filename.c_str(),
                    add_random(item.LeftEyeX), add_random(item.LeftEyeY),
                    add_random(item.RightEyeX), add_random(item.RightEyeY),
                    add_random(item.NoseX), add_random(item.NoseY),
                    add_random(item.MouthX), add_random(item.MouthY));
            }
            fclose(fout);

            CStringW strFile(pszFile);
            INT ich = strFile.ReverseFind(L'\\');
            CStringW strPath = strFile.Left(ich);
            m_strPath = strPath;
            return TRUE;
        }
        return FALSE;
    }
};

CMainWindow::CMainWindow()
{
}

CMainWindow::~CMainWindow()
{
}

VOID CMainWindow::ResetSettings()
{
    // TODO: Reset settings
}

BOOL CMainWindow::LoadSettings()
{
    ResetSettings();

    CRegKey appKey;
    LONG error;

    error = appKey.Open(HKEY_CURRENT_USER, TEXT("Software\\ReactOS\\PicoDataEdit"));
    if (error)
        return FALSE;

    // TODO: Load settings

    return TRUE;
}

BOOL CMainWindow::SaveSettings()
{
    CRegKey companyKey, appKey;
    LONG error;

    error = companyKey.Create(HKEY_CURRENT_USER, TEXT("Software\\ReactOS"));
    if (error)
        return FALSE;

    error = appKey.Create(companyKey, TEXT("PicoDataEdit"));
    if (error)
        return FALSE;

    // TODO: Save settings

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

INT WINAPI
WinMain(HINSTANCE   hInstance,
        HINSTANCE   hPrevInstance,
        LPSTR       lpCmdLine,
        INT         nCmdShow)
{
    InitCommonControls();

    CMainWindow mainWnd;
    mainWnd.LoadSettings();

    if (mainWnd.DoModal(NULL, 0) == IDOK)
    {
        // TODO: Do something
    }

    mainWnd.SaveSettings();

    return 0;
}
