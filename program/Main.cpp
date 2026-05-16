#include "Main.h"
#include "Game.h"
#include "DxLib.h" // DXライブラリの関数を確実に呼ぶために追加

char KeyBuffer[256];
int KeyFrame[256];
int MouseLeftFrame;
int MouseRightFrame;

//---------------------------------------------------------------------------------
//    WinMain
//---------------------------------------------------------------------------------
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    int Time;

    SetOutApplicationLogValidFlag(FALSE);
    ChangeWindowMode(TRUE);
    SetMainWindowText("パズルボブル風アプリ");

    // ★ここで縦画面（横480, 縦640）に強制設定
    SetGraphMode(960, 640, 32);

    SetDoubleStartValidFlag(TRUE);
    SetAlwaysRunFlag(TRUE);

    if (DxLib_Init() == -1)    return -1;

    SetDrawScreen(DX_SCREEN_BACK);
    SetTransColor(255, 0, 255);
    srand(GetNowCount() % RAND_MAX);

    for (int i = 0; i < 256; i++) {
        KeyFrame[i] = 0;
    }
    MouseLeftFrame = 0;
    MouseRightFrame = 0;

    GameInit();

    while (TRUE)
    {
        Time = GetNowCount();
        ClearDrawScreen();

        // 【テスト用背景変更】画面が動いているか判別するため、背景を暗い青緑にします
        DrawBox(0, 0, 480, 640, GetColor(30, 50, 70), TRUE);

        GetHitKeyStateAll(KeyBuffer);

        for (int i = 0; i < 256; i++) {
            if (KeyBuffer[i])    KeyFrame[i]++;
            else                KeyFrame[i] = 0;
        }

        if (CheckMouseInput(MOUSE_INPUT_LEFT))    MouseLeftFrame++;
        else                                         MouseLeftFrame = 0;

        if (CheckMouseInput(MOUSE_INPUT_RIGHT))    MouseRightFrame++;
        else                                          MouseRightFrame = 0;

        // ゲームの処理
        GameUpdate();
        GameRender();

        // ★【テスト用文字】Main.cppが生きているか確認用
        //DrawString(10, 10, "Main Loop Running (480x640)", GetColor(255, 255, 255));

        ScreenFlip();
        while (GetNowCount() - Time < 17) {}
        if (ProcessMessage())    break;
        if (CheckHitKey(KEY_INPUT_ESCAPE))    break;
    }

    GameExit();

    DxLib_End();
    return 0;
}

//---------------------------------------------------------------------------------
//    キーが押された瞬間を取得する
//---------------------------------------------------------------------------------
bool PushHitKey(int key)
{
    if (KeyFrame[key] == 1) {
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------
//    マウスが押されているかを取得する
//---------------------------------------------------------------------------------
bool CheckMouseInput(int button)
{
    if (GetMouseInput() & button) {
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------
//    マウスが押された瞬間を取得する
//---------------------------------------------------------------------------------
bool PushMouseInput(int button)
{
    if (button & MOUSE_INPUT_LEFT) {
        if (MouseLeftFrame == 1) {
            return true;
        }
    }
    if (button & MOUSE_INPUT_RIGHT) {
        if (MouseRightFrame == 1) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------
//    マウスの座標を取得する
//---------------------------------------------------------------------------------
int GetMouseX()
{
    int mouse_x;
    int mouse_y;
    GetMousePoint(&mouse_x, &mouse_y);
    return mouse_x;
}
int GetMouseY()
{
    int mouse_x;
    int mouse_y;
    GetMousePoint(&mouse_x, &mouse_y);
    return mouse_y;
}