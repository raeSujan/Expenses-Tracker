/* =========================================================
   PERSONAL EXPENSE TRACKER  -  OOP + raylib GUI

   Build (Linux):
     g++ main.cpp -o tracker -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
   Build (Windows / MinGW):
     g++ main.cpp -o tracker.exe -lraylib -lopengl32 -lgdi32 -lwinmm
   Build (macOS):
     g++ main.cpp -o tracker -lraylib -framework OpenGL -framework Cocoa -framework IOKit

   Data files (same format as the console version):
     user.dat, categories.dat, transactions.dat
   ========================================================= */

#include "raylib.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <memory>
using namespace std;

static const int WIN_W = 900;
static const int WIN_H = 680;

/* ---------------------------------------------------------
   Theme colours
   --------------------------------------------------------- */
namespace Theme
{
    const Color BG     = {18, 22, 32, 255};
    const Color HEADER = {12, 15, 24, 255};
    const Color PANEL  = {30, 36, 50, 255};
    const Color PANEL2 = {44, 53, 74, 255};
    const Color INPUT  = {20, 24, 36, 255};
    const Color BORDER = {70, 82, 110, 255};
    const Color ACCENT = {64, 132, 246, 255};
    const Color BTN_GREEN  = {40, 160, 100, 255};
    const Color BTN_RED  = {214, 68, 78, 255};
    const Color TEXT   = {236, 240, 247, 255};
    const Color MUTED  = {145, 155, 176, 255};
    const Color GOOD   = {90, 215, 140, 255};
    const Color BAD    = {255, 110, 120, 255};
}

/* ---------------------------------------------------------
   UI : small text-drawing helpers
   --------------------------------------------------------- */
namespace UI
{
    void Text(const string& s, float x, float y, int size, Color c)
    {
        DrawText(s.c_str(), (int)x, (int)y, size, c);
    }
    void TextCentered(const string& s, float cx, float y, int size, Color c)
    {
        int w = MeasureText(s.c_str(), size);
        DrawText(s.c_str(), (int)(cx - w / 2.0f), (int)y, size, c);
    }
    void TextRight(const string& s, float rx, float y, int size, Color c)
    {
        int w = MeasureText(s.c_str(), size);
        DrawText(s.c_str(), (int)(rx - w), (int)y, size, c);
    }
    bool EnterPressed()
    {
        return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    }
}

/* =========================================================
   MODEL CLASSES  (no screen code in here)
   ========================================================= */

/* ---------------- InputHelper : validation ---------------- */
class InputHelper
{
public:
    static string toLower(string s)
    {
        for (size_t i = 0; i < s.size(); i++)
            s[i] = (char)tolower((unsigned char)s[i]);
        return s;
    }

    static string trim(const string& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && isspace((unsigned char)s[a])) a++;
        while (b > a && isspace((unsigned char)s[b - 1])) b--;
        return s.substr(a, b - a);
    }

    static bool lettersOnly(const string& s)
    {
        if (s.empty()) return false;
        bool hasLetter = false;
        for (size_t i = 0; i < s.size(); i++)
        {
            if (isalpha((unsigned char)s[i])) hasLetter = true;
            else if (s[i] != ' ') return false;
        }
        return hasLetter;
    }

    static void getCurrentMonthYear(int& month, int& year)
    {
        time_t now = time(0);
        tm* t = localtime(&now);
        month = t->tm_mon + 1;
        year = t->tm_year + 1900;
    }

    static bool isLeap(int y)
    {
        return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
    }

    static int daysInMonth(int m, int y)
    {
        int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (m == 2 && isLeap(y)) return 29;
        return d[m - 1];
    }

    // date format: DD/MM/YYYY   (error text goes into 'err')
    static bool validDate(const string& s, int& d, int& m, int& y, string& err)
    {
        if (s.size() != 10 || s[2] != '/' || s[5] != '/')
        {
            err = "Wrong format. Use DD/MM/YYYY (example: 05/09/2026).";
            return false;
        }
        for (size_t i = 0; i < s.size(); i++)
        {
            if (i == 2 || i == 5) continue;
            if (!isdigit((unsigned char)s[i]))
            {
                err = "Date must contain only numbers (no alphabets or symbols).";
                return false;
            }
        }
        d = atoi(s.substr(0, 2).c_str());
        m = atoi(s.substr(3, 2).c_str());
        y = atoi(s.substr(6, 4).c_str());

        if (m < 1 || m > 12 || d < 1 || d > daysInMonth(m, y))
        {
            err = "This date does not exist.";
            return false;
        }

        int curM, curY;
        getCurrentMonthYear(curM, curY);
        if (m != curM || y != curY)
        {
            ostringstream o;
            o << "Date must be within the current month and year ("
              << setfill('0') << setw(2) << curM << "/" << curY << ").";
            err = o.str();
            return false;
        }
        return true;
    }

    // type: 'I' or 'E'. Returns signed amount in 'out'
    static bool parseAmount(const string& s, char type, double& out, string& err)
    {
        if (s.empty())
        {
            err = "Amount cannot be empty.";
            return false;
        }
        if (type == 'I' && s[0] == '-')
        {
            err = "Income amount cannot start with '-'.";
            return false;
        }
        if (type == 'E' && s[0] == '+')
        {
            err = "Expense amount cannot start with '+'.";
            return false;
        }

        size_t i = 0;
        if (s[0] == '-' || s[0] == '+') i = 1;
        int dots = 0, digits = 0;
        for (; i < s.size(); i++)
        {
            if (isdigit((unsigned char)s[i])) digits++;
            else if (s[i] == '.' && dots == 0) dots++;
            else
            {
                err = "Amount must be a valid number.";
                return false;
            }
        }
        if (digits == 0)
        {
            err = "Amount must be a valid number.";
            return false;
        }

        double v = atof(s.c_str());
        if (v < 0) v = -v;
        if (v == 0)
        {
            err = "Amount cannot be zero.";
            return false;
        }
        out = (type == 'E') ? -v : v;
        return true;
    }

    // "+123.45" / "-50.00"
    static string money(double v)
    {
        ostringstream o;
        o << fixed << setprecision(2) << (v >= 0 ? "+" : "") << v;
        return o.str();
    }

    // "05/09/2026"
    static string dateText(int d, int m, int y)
    {
        ostringstream o;
        o << setfill('0') << setw(2) << d << "/" << setw(2) << m << "/" << y;
        return o.str();
    }
};

/* ---------------- UserAccount : register / login data ---------------- */
class UserAccount
{
private:
    string userFile;

    // simple hash so the password is not saved as plain text
    static unsigned long hashPassword(const string& s)
    {
        unsigned long h = 5381;
        for (size_t i = 0; i < s.size(); i++)
            h = h * 33 + (unsigned char)s[i];
        return h;
    }

public:
    UserAccount() : userFile("user.dat") {}

    bool exists() const
    {
        ifstream f(userFile.c_str());
        return f.good();
    }

    static bool validPassword(const string& p, string& msg)
    {
        bool hasDigit = false, hasSpecial = false;

        if (p.size() < 5)
        {
            msg = "Password must be at least 5 characters long.";
            return false;
        }
        for (size_t i = 0; i < p.size(); i++)
        {
            if (isspace((unsigned char)p[i]))
            {
                msg = "Password must not contain spaces.";
                return false;
            }
            if (isdigit((unsigned char)p[i])) hasDigit = true;
            else if (!isalpha((unsigned char)p[i])) hasSpecial = true;
        }
        if (!hasDigit)
        {
            msg = "Password must contain at least one number.";
            return false;
        }
        if (!hasSpecial)
        {
            msg = "Password must contain at least one special character (e.g. @ # $ !).";
            return false;
        }
        return true;
    }

    static bool validUsername(const string& u)
    {
        if (u.empty()) return false;
        for (size_t i = 0; i < u.size(); i++)
            if (isspace((unsigned char)u[i])) return false;
        return true;
    }

    void save(const string& username, const string& password) const
    {
        ofstream f(userFile.c_str());
        f << username << "\n" << hashPassword(password) << "\n";
    }

    bool verify(const string& username, const string& password) const
    {
        string savedUser;
        unsigned long savedHash = 0;
        ifstream f(userFile.c_str());
        f >> savedUser >> savedHash;
        return username == savedUser && hashPassword(password) == savedHash;
    }
};

/* ---------------- Transaction : one income/expense record ---------------- */
class Transaction
{
private:
    char type;          // 'I' = income, 'E' = expense
    int day, month, year;
    double amount;      // income = positive, expense = negative
    string category;

public:
    Transaction() : type('I'), day(0), month(0), year(0), amount(0) {}

    Transaction(char t, int d, int m, int y, double a, const string& c)
        : type(t), day(d), month(m), year(y), amount(a), category(c) {}

    char getType() const { return type; }
    int getDay() const { return day; }
    int getMonth() const { return month; }
    int getYear() const { return year; }
    double getAmount() const { return amount; }
    const string& getCategory() const { return category; }

    void writeTo(ostream& os) const
    {
        os << type << " " << day << " " << month << " " << year << " "
           << amount << " " << category << "\n";
    }

    static Transaction fromLine(const string& line)
    {
        istringstream ss(line);
        Transaction t;
        ss >> t.type >> t.day >> t.month >> t.year >> t.amount;
        string cat;
        getline(ss, cat);
        t.category = InputHelper::trim(cat);
        return t;
    }

    static bool byDate(const Transaction& a, const Transaction& b)
    {
        return a.day < b.day;
    }
};

/* ---------------- CategoryManager ---------------- */
class CategoryManager
{
private:
    vector<string> incomeCats;
    vector<string> expenseCats;
    string catFile;

    vector<string>& listFor(char type)
    {
        return (type == 'I') ? incomeCats : expenseCats;
    }

public:
    CategoryManager() : catFile("categories.dat") {}

    const vector<string>& getList(char type) const
    {
        return (type == 'I') ? incomeCats : expenseCats;
    }

    static int find(const vector<string>& cats, const string& name)
    {
        for (size_t i = 0; i < cats.size(); i++)
            if (InputHelper::toLower(cats[i]) == InputHelper::toLower(name)) return (int)i;
        return -1;
    }

    // returns false and fills 'err' when the name is not acceptable
    bool add(char type, const string& rawName, string& err)
    {
        string name = InputHelper::trim(rawName);
        vector<string>& cats = listFor(type);
        if (!InputHelper::lettersOnly(name))
        {
            err = "Category must contain only letters (no numbers or special characters).";
            return false;
        }
        if (find(cats, name) >= 0)
        {
            err = "This category already exists.";
            return false;
        }
        cats.push_back(name);
        save();
        return true;
    }

    string remove(char type, int index)
    {
        vector<string>& cats = listFor(type);
        string removed = cats[index];
        cats.erase(cats.begin() + index);
        save();
        return removed;
    }

    void save() const
    {
        ofstream f(catFile.c_str());
        for (size_t i = 0; i < incomeCats.size(); i++)  f << "I " << incomeCats[i] << "\n";
        for (size_t i = 0; i < expenseCats.size(); i++) f << "E " << expenseCats[i] << "\n";
    }

    void load()
    {
        incomeCats.clear();
        expenseCats.clear();
        ifstream f(catFile.c_str());
        string line;
        while (getline(f, line))
        {
            if (line.size() < 3) continue;
            string name = InputHelper::trim(line.substr(2));
            if (line[0] == 'I') incomeCats.push_back(name);
            else if (line[0] == 'E') expenseCats.push_back(name);
        }
    }
};

/* ---------------- TransactionManager ---------------- */
class TransactionManager
{
private:
    vector<Transaction> transactions;
    string transFile;

public:
    TransactionManager() : transFile("transactions.dat") {}

    void add(const Transaction& t)
    {
        transactions.push_back(t);
        save();
    }

    void save() const
    {
        ofstream f(transFile.c_str());
        for (size_t i = 0; i < transactions.size(); i++)
            transactions[i].writeTo(f);
    }

    void load()
    {
        transactions.clear();
        ifstream f(transFile.c_str());
        string line;
        while (getline(f, line))
        {
            if (line.empty()) continue;
            transactions.push_back(Transaction::fromLine(line));
        }
    }

    double totalBalance() const
    {
        double total = 0;
        for (size_t i = 0; i < transactions.size(); i++)
            total += transactions[i].getAmount();
        return total;
    }

    // transactions of the current month, sorted by day
    vector<Transaction> currentMonth() const
    {
        int curM, curY;
        InputHelper::getCurrentMonthYear(curM, curY);

        vector<Transaction> list;
        for (size_t i = 0; i < transactions.size(); i++)
            if (transactions[i].getMonth() == curM && transactions[i].getYear() == curY)
                list.push_back(transactions[i]);

        stable_sort(list.begin(), list.end(), Transaction::byDate);
        return list;
    }
};

/* ---------------- AppContext : shared data + navigation ---------------- */
enum ScreenId
{
    SCR_REGISTER, SCR_LOGIN, SCR_MENU, SCR_ADD,
    SCR_BUDGET, SCR_VIEWCAT, SCR_EDITCAT, SCR_SUMMARY, SCR_COUNT
};

struct AppContext
{
    UserAccount account;
    CategoryManager cats;
    TransactionManager trans;
    string currentUser;
    string notice;          // one-time message shown on the next screen
    int nextScreen;
    bool quit;

    AppContext() : nextScreen(-1), quit(false) {}
    void Go(int id) { nextScreen = id; }
};

/* =========================================================
   WIDGET CLASSES
   ========================================================= */

/* ---------------- Button ---------------- */
class Button
{
private:
    Rectangle rect;
    string label;
    Color color;
    int fontSize;
    bool hasClip;
    Rectangle clip;

public:
    Button(Rectangle r, const string& text, Color c = Theme::ACCENT, int size = 22)
        : rect(r), label(text), color(c), fontSize(size), hasClip(false), clip{0, 0, 0, 0} {}

    void SetClip(Rectangle c) { clip = c; hasClip = true; }

    // draws the button, returns true on the frame it is clicked
    bool Frame()
    {
        Vector2 m = GetMousePosition();
        bool hover = CheckCollisionPointRec(m, rect) &&
                     (!hasClip || CheckCollisionPointRec(m, clip));
        bool down = hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        Color c = color;
        if (down) c = ColorBrightness(color, -0.25f);
        else if (hover) c = ColorBrightness(color, 0.18f);

        DrawRectangleRounded(rect, 0.25f, 8, c);
        int w = MeasureText(label.c_str(), fontSize);
        DrawText(label.c_str(), (int)(rect.x + (rect.width - w) / 2),
                 (int)(rect.y + (rect.height - fontSize) / 2), fontSize, Theme::TEXT);

        if (hover) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    }
};

/* ---------------- TextBox ---------------- */
class TextBox
{
private:
    Rectangle rect;
    string label, placeholder, text;
    bool masked, focused;
    size_t maxLen;
    double nextRepeat;

public:
    TextBox(Rectangle r, const string& lab, const string& ph = "",
            bool mask = false, size_t maxL = 30)
        : rect(r), label(lab), placeholder(ph), masked(mask),
          focused(false), maxLen(maxL), nextRepeat(0) {}

    const string& GetText() const { return text; }
    void SetText(const string& s) { text = s; }
    void Clear() { text.clear(); }
    void SetFocus(bool f) { focused = f; }
    bool HasFocus() const { return focused; }
    void SetPlaceholder(const string& p) { placeholder = p; }

    void Update()
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            focused = CheckCollisionPointRec(GetMousePosition(), rect);
        if (!focused) return;

        int ch;
        while ((ch = GetCharPressed()) > 0)
            if (ch >= 32 && ch <= 126 && text.size() < maxLen)
                text.push_back((char)ch);

        if (IsKeyPressed(KEY_BACKSPACE))
        {
            if (!text.empty()) text.erase(text.size() - 1);
            nextRepeat = GetTime() + 0.4;
        }
        else if (IsKeyDown(KEY_BACKSPACE) && GetTime() >= nextRepeat)
        {
            if (!text.empty()) text.erase(text.size() - 1);
            nextRepeat = GetTime() + 0.04;
        }
    }

    void Draw() const
    {
        UI::Text(label, rect.x, rect.y - 24, 18, Theme::MUTED);

        DrawRectangleRounded(rect, 0.15f, 8, focused ? Theme::ACCENT : Theme::BORDER);
        Rectangle inner = {rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4};
        DrawRectangleRounded(inner, 0.15f, 8, Theme::INPUT);

        string shown = masked ? string(text.size(), '*') : text;
        int fs = 20;
        int ty = (int)(rect.y + (rect.height - fs) / 2);

        if (shown.empty() && !focused)
        {
            UI::Text(placeholder, rect.x + 12, (float)ty, fs, Color{90, 100, 125, 255});
            return;
        }

        int w = MeasureText(shown.c_str(), fs);
        float shift = (float)max(0, w - (int)(rect.width - 28));

        BeginScissorMode((int)rect.x + 4, (int)rect.y + 2, (int)rect.width - 8, (int)rect.height - 4);
        UI::Text(shown, rect.x + 12 - shift, (float)ty, fs, Theme::TEXT);
        if (focused && ((int)(GetTime() * 2) % 2 == 0))
            DrawRectangle((int)(rect.x + 12 - shift + w + 2), (int)rect.y + 9, 2,
                          (int)rect.height - 18, Theme::TEXT);
        EndScissorMode();
    }
};

/* ---------------- Scroller : mouse-wheel scrolling for lists ---------------- */
class Scroller
{
private:
    float offset;

public:
    Scroller() : offset(0) {}
    void Reset() { offset = 0; }

    float Update(Rectangle view, float contentH)
    {
        if (CheckCollisionPointRec(GetMousePosition(), view))
            offset -= GetMouseWheelMove() * 40.0f;
        float maxOff = max(0.0f, contentH - view.height);
        if (offset > maxOff) offset = maxOff;
        if (offset < 0) offset = 0;

        // scroll bar
        if (contentH > view.height)
        {
            float barH = view.height * (view.height / contentH);
            float barY = view.y + (view.height - barH) * (offset / maxOff);
            DrawRectangleRounded(Rectangle{view.x + view.width + 4, barY, 6, barH},
                                 0.5f, 4, Theme::BORDER);
        }
        return offset;
    }
};

/* =========================================================
   SCREEN CLASSES  (each screen = one class)
   ========================================================= */
class Screen
{
protected:
    AppContext& ctx;
    vector<TextBox*> fields;
    string msg;
    bool msgOk;

    void SetMsg(const string& m, bool ok) { msg = m; msgOk = ok; }

    // updates all text boxes, Tab moves to the next one
    void UpdateFields()
    {
        for (size_t i = 0; i < fields.size(); i++) fields[i]->Update();

        if (IsKeyPressed(KEY_TAB) && !fields.empty())
        {
            int cur = -1;
            for (size_t i = 0; i < fields.size(); i++)
                if (fields[i]->HasFocus()) cur = (int)i;
            for (size_t i = 0; i < fields.size(); i++) fields[i]->SetFocus(false);
            fields[(cur + 1) % fields.size()]->SetFocus(true);
        }

        bool any = false;
        for (size_t i = 0; i < fields.size(); i++)
            if (fields[i]->HasFocus()) any = true;
        if (!any) while (GetCharPressed() > 0) {}
    }

    void DrawFields() const
    {
        for (size_t i = 0; i < fields.size(); i++) fields[i]->Draw();
    }

    void ClearFields()
    {
        for (size_t i = 0; i < fields.size(); i++)
        {
            fields[i]->Clear();
            fields[i]->SetFocus(false);
        }
        if (!fields.empty()) fields[0]->SetFocus(true);
    }

    void DrawHeader(const string& title) const
    {
        DrawRectangle(0, 0, WIN_W, 70, Theme::HEADER);
        DrawRectangle(0, 68, WIN_W, 2, Theme::ACCENT);
        UI::Text("PERSONAL EXPENSE TRACKER", 30, 22, 28, Theme::TEXT);
        UI::TextRight(title, WIN_W - 30, 28, 20, Theme::MUTED);
    }

    void DrawCard(Rectangle r) const
    {
        DrawRectangleRounded(r, 0.04f, 8, Theme::PANEL);
    }

    void DrawMsg(float cx, float y) const
    {
        if (msg.empty()) return;
        string s = msg;
        if (MeasureText(s.c_str(), 18) > 760)
        {
            while (MeasureText((s + "...").c_str(), 18) > 760) s.erase(s.size() - 1);
            s += "...";
        }
        UI::TextCentered(s, cx, y, 18, msgOk ? Theme::GOOD : Theme::BAD);
    }

    bool BackButton()
    {
        if (Button(Rectangle{40, 610, 200, 46}, "< Back to Menu", Theme::PANEL2, 20).Frame())
        {
            ctx.Go(SCR_MENU);
            return true;
        }
        return false;
    }

    // Income / Expense tab buttons; returns true when the tab changed
    bool DrawTabs(char& type, float y, const string& incomeLabel, const string& expenseLabel)
    {
        bool changed = false;
        if (Button(Rectangle{60, y, 260, 44}, incomeLabel,
                   type == 'I' ? Theme::BTN_GREEN : Theme::PANEL2, 20).Frame() && type != 'I')
        { type = 'I'; changed = true; }
        if (Button(Rectangle{335, y, 260, 44}, expenseLabel,
                   type == 'E' ? Theme::BTN_RED : Theme::PANEL2, 20).Frame() && type != 'E')
        { type = 'E'; changed = true; }
        return changed;
    }

public:
    Screen(AppContext& c) : ctx(c), msgOk(true) {}
    virtual ~Screen() {}
    virtual void OnEnter() {}
    virtual void Frame() = 0;     // handle input + draw one frame
};

/* ---------------- Register ---------------- */
class RegisterScreen : public Screen
{
private:
    TextBox user, pass, confirm;

public:
    RegisterScreen(AppContext& c)
        : Screen(c),
          user(Rectangle{250, 225, 400, 44}, "Username (no spaces)", "Choose a username"),
          pass(Rectangle{250, 305, 400, 44}, "Password", "Choose a password", true),
          confirm(Rectangle{250, 385, 400, 44}, "Confirm password", "Repeat the password", true)
    {
        fields.push_back(&user);
        fields.push_back(&pass);
        fields.push_back(&confirm);
    }

    void OnEnter() override { ClearFields(); msg.clear(); }

    void Frame() override
    {
        UpdateFields();
        DrawHeader("REGISTER");
        DrawCard(Rectangle{130, 95, 640, 530});
        UI::TextCentered("CREATE ACCOUNT", 450, 118, 30, Theme::TEXT);
        UI::TextCentered("No account found. Please register first.", 450, 160, 18, Theme::MUTED);
        DrawFields();

        bool submit = Button(Rectangle{250, 450, 400, 50}, "Register", Theme::BTN_GREEN).Frame();
        if (UI::EnterPressed()) submit = true;
        DrawMsg(450, 520);
        UI::TextCentered("Rules: 5+ characters, no spaces, 1 number, 1 special character (@ # $ !)",
                         450, 570, 16, Theme::MUTED);

        if (submit)
        {
            string m;
            if (!UserAccount::validUsername(user.GetText()))
                SetMsg("Invalid username. Try again.", false);
            else if (!UserAccount::validPassword(pass.GetText(), m))
                SetMsg(m, false);
            else if (confirm.GetText() != pass.GetText())
                SetMsg("Passwords do not match. Try again.", false);
            else
            {
                ctx.account.save(user.GetText(), pass.GetText());
                ctx.notice = "Registration successful! Please login now.";
                ctx.Go(SCR_LOGIN);
            }
        }
    }
};

/* ---------------- Login ---------------- */
class LoginScreen : public Screen
{
private:
    TextBox user, pass;
    int attempts;
    bool locked;

public:
    LoginScreen(AppContext& c)
        : Screen(c),
          user(Rectangle{250, 215, 400, 44}, "Username", "Enter your username"),
          pass(Rectangle{250, 305, 400, 44}, "Password", "Enter your password", true),
          attempts(3), locked(false)
    {
        fields.push_back(&user);
        fields.push_back(&pass);
    }

    void OnEnter() override
    {
        ClearFields();
        attempts = 3;
        locked = false;
        msg.clear();
        if (!ctx.notice.empty()) { SetMsg(ctx.notice, true); ctx.notice.clear(); }
    }

    void Frame() override
    {
        if (!locked) UpdateFields();
        DrawHeader("LOGIN");
        DrawCard(Rectangle{130, 95, 640, 480});
        UI::TextCentered("LOGIN", 450, 118, 30, Theme::TEXT);
        UI::TextCentered("Enter your username and password", 450, 160, 18, Theme::MUTED);
        DrawFields();

        bool submit = false;
        if (!locked)
        {
            submit = Button(Rectangle{250, 380, 400, 50}, "Login", Theme::ACCENT).Frame();
            if (UI::EnterPressed()) submit = true;
        }
        DrawMsg(450, 450);

        if (locked)
        {
            UI::TextCentered("Too many failed attempts. Program closed.", 450, 480, 20, Theme::BAD);
            if (Button(Rectangle{250, 515, 400, 46}, "Exit", Theme::BTN_RED).Frame())
                ctx.quit = true;
        }

        if (submit)
        {
            if (ctx.account.verify(user.GetText(), pass.GetText()))
            {
                ctx.currentUser = user.GetText();
                ctx.notice = "Login successful. Welcome, " + ctx.currentUser + "!";
                ctx.cats.load();
                ctx.trans.load();
                ctx.Go(SCR_MENU);
            }
            else
            {
                attempts--;
                SetMsg("Wrong username or password. Attempts left: " + to_string(attempts), false);
                pass.Clear();
                if (attempts == 0) locked = true;
            }
        }
    }
};

/* ---------------- Main menu ---------------- */
class MenuScreen : public Screen
{
private:
    bool bye;
    double byeTime;

public:
    MenuScreen(AppContext& c) : Screen(c), bye(false), byeTime(0) {}

    void OnEnter() override
    {
        msg.clear();
        if (!ctx.notice.empty()) { SetMsg(ctx.notice, true); ctx.notice.clear(); }
    }

    void Frame() override
    {
        DrawHeader("MAIN MENU");
        UI::TextCentered("EXPENSE TRACKER MENU", 450, 100, 32, Theme::TEXT);
        DrawMsg(450, 148);

        if (bye)
        {
            UI::TextCentered("Goodbye!", 450, 300, 60, Theme::GOOD);
            if (GetTime() - byeTime > 1.2) ctx.quit = true;
            return;
        }

        const float W = 300, H = 90;
        const float X1 = 130, X2 = 470;
        const float Y1 = 195, Y2 = 305, Y3 = 415;

        if (Button(Rectangle{X1, Y1, W, H}, "1. Add Transaction", Theme::BTN_GREEN, 24).Frame())
            ctx.Go(SCR_ADD);
        if (Button(Rectangle{X2, Y1, W, H}, "2. View Budget", Theme::ACCENT, 24).Frame())
            ctx.Go(SCR_BUDGET);
        if (Button(Rectangle{X1, Y2, W, H}, "3. View Categories", Theme::ACCENT, 24).Frame())
            ctx.Go(SCR_VIEWCAT);
        if (Button(Rectangle{X2, Y2, W, H}, "4. Edit Categories", Theme::ACCENT, 24).Frame())
            ctx.Go(SCR_EDITCAT);
        if (Button(Rectangle{X1, Y3, W, H}, "5. Monthly Summary", Theme::ACCENT, 24).Frame())
            ctx.Go(SCR_SUMMARY);
        if (Button(Rectangle{X2, Y3, W, H}, "6. Exit", Theme::BTN_RED, 24).Frame())
        {
            bye = true;
            byeTime = GetTime();
        }
    }
};

/* ---------------- Add transaction ---------------- */
class AddTransactionScreen : public Screen
{
private:
    char type;
    TextBox date, amount, category;
    Scroller scroll;

    void SwitchType(char t)
    {
        type = t;
        category.Clear();
        msg.clear();
        scroll.Reset();
    }

public:
    AddTransactionScreen(AppContext& c)
        : Screen(c), type('I'),
          date(Rectangle{70, 235, 380, 44}, "Date (DD/MM/YYYY)", "e.g. 05/09/2026", false, 10),
          amount(Rectangle{70, 315, 380, 44}, "Amount", "e.g. 1000", false, 15),
          category(Rectangle{70, 395, 380, 44}, "Category", "Type or click one on the right")
    {
        fields.push_back(&date);
        fields.push_back(&amount);
        fields.push_back(&category);
    }

    void OnEnter() override { ClearFields(); msg.clear(); scroll.Reset(); }

    void Frame() override
    {
        DrawHeader("ADD TRANSACTION");
        DrawCard(Rectangle{40, 90, 820, 505});

        // type buttons
        if (Button(Rectangle{70, 110, 190, 46}, "1. Income",
                   type == 'I' ? Theme::BTN_GREEN : Theme::PANEL2).Frame() && type != 'I')
            SwitchType('I');
        if (Button(Rectangle{275, 110, 190, 46}, "2. Expense",
                   type == 'E' ? Theme::BTN_RED : Theme::PANEL2).Frame() && type != 'E')
            SwitchType('E');

        const vector<string>& cats = ctx.cats.getList(type);

        if (cats.empty())
        {
            DrawRectangleRounded(Rectangle{70, 190, 760, 90}, 0.15f, 8, Color{70, 35, 42, 255});
            UI::TextCentered(string("No ") + (type == 'I' ? "income" : "expense") +
                             " categories yet. Add one first (Menu option 4).",
                             450, 224, 20, Theme::BAD);
            if (Button(Rectangle{70, 300, 300, 46}, "Go to Edit Categories", Theme::ACCENT, 20).Frame())
                ctx.Go(SCR_EDITCAT);
            BackButton();
            return;
        }

        UpdateFields();
        amount.SetPlaceholder(type == 'I' ? "e.g. 1000" : "e.g. 250  or  -250");
        DrawFields();

        // category list on the right
        UI::Text("Your categories (click to select)", 490, 172, 18, Theme::MUTED);
        Rectangle view = {490, 200, 330, 300};
        float off = scroll.Update(view, cats.size() * 44.0f);
        BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
        for (size_t i = 0; i < cats.size(); i++)
        {
            Rectangle r = {view.x, view.y + i * 44.0f - off, view.width, 38};
            if (r.y + r.height < view.y || r.y > view.y + view.height) continue;
            Button b(r, cats[i], Theme::PANEL2, 20);
            b.SetClip(view);
            if (b.Frame()) category.SetText(cats[i]);
        }
        EndScissorMode();

        bool submit = Button(Rectangle{70, 465, 380, 50}, "Save Transaction",
                             type == 'I' ? Theme::BTN_GREEN : Theme::BTN_RED).Frame();
        if (UI::EnterPressed()) submit = true;
        DrawMsg(450, 545);

        if (submit)
        {
            string err;
            int d = 0, m = 0, y = 0;
            double amt = 0;

            if (!InputHelper::validDate(InputHelper::trim(date.GetText()), d, m, y, err))
                SetMsg(err, false);
            else if (!InputHelper::parseAmount(InputHelper::trim(amount.GetText()), type, amt, err))
                SetMsg(err, false);
            else
            {
                int idx = CategoryManager::find(cats, InputHelper::trim(category.GetText()));
                if (idx < 0)
                {
                    string list;
                    for (size_t i = 0; i < cats.size(); i++)
                        list += cats[i] + (i + 1 < cats.size() ? ", " : "");
                    SetMsg("Category not found. Your categories are: " + list, false);
                }
                else
                {
                    ctx.trans.add(Transaction(type, d, m, y, amt, cats[idx]));
                    SetMsg(string(type == 'I' ? "Income" : "Expense") + " recorded successfully.", true);
                    ClearFields();
                }
            }
        }
        BackButton();
    }
};

/* ---------------- View budget ---------------- */
class BudgetScreen : public Screen
{
private:
    double total;

public:
    BudgetScreen(AppContext& c) : Screen(c), total(0) {}

    void OnEnter() override { total = ctx.trans.totalBalance(); }

    void Frame() override
    {
        DrawHeader("VIEW BUDGET");
        DrawCard(Rectangle{120, 130, 660, 360});
        UI::TextCentered("TOTAL BALANCE", 450, 160, 32, Theme::TEXT);
        DrawRectangle(300, 210, 300, 2, Theme::BORDER);

        string s = "Balance: " + InputHelper::money(total);
        UI::TextCentered(s, 450, 275, 52, total >= 0 ? Theme::GOOD : Theme::BAD);
        UI::TextCentered("Sum of all income and expense transactions", 450, 400, 18, Theme::MUTED);
        BackButton();
    }
};

/* ---------------- View categories ---------------- */
class ViewCategoriesScreen : public Screen
{
private:
    char type;
    Scroller scroll;

public:
    ViewCategoriesScreen(AppContext& c) : Screen(c), type('I') {}

    void OnEnter() override { scroll.Reset(); }

    void Frame() override
    {
        DrawHeader("VIEW CATEGORIES");
        if (DrawTabs(type, 100, "1. Income categories", "2. Expense categories"))
            scroll.Reset();

        DrawCard(Rectangle{40, 165, 820, 430});
        UI::Text(string(type == 'I' ? "Income" : "Expense") + " categories:", 70, 182, 24, Theme::TEXT);

        const vector<string>& cats = ctx.cats.getList(type);
        if (cats.empty())
            UI::TextCentered("(none added yet)", 450, 330, 22, Theme::MUTED);

        Rectangle view = {70, 225, 760, 350};
        float off = scroll.Update(view, cats.size() * 44.0f);
        BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
        for (size_t i = 0; i < cats.size(); i++)
        {
            float y = view.y + i * 44.0f - off;
            DrawRectangleRounded(Rectangle{view.x, y, view.width, 38}, 0.2f, 6,
                                 i % 2 ? Theme::PANEL2 : Color{38, 46, 64, 255});
            UI::Text(to_string(i + 1) + ".  " + cats[i], view.x + 16, y + 9, 20, Theme::TEXT);
        }
        EndScissorMode();
        BackButton();
    }
};

/* ---------------- Edit categories (add / remove) ---------------- */
class EditCategoriesScreen : public Screen
{
private:
    char type;
    TextBox name;
    Scroller scroll;

public:
    EditCategoriesScreen(AppContext& c)
        : Screen(c), type('I'),
          name(Rectangle{70, 235, 560, 44}, "New category name (letters only)", "e.g. Salary")
    {
        fields.push_back(&name);
    }

    void OnEnter() override { ClearFields(); msg.clear(); scroll.Reset(); }

    void Frame() override
    {
        UpdateFields();
        DrawHeader("EDIT CATEGORIES");
        if (DrawTabs(type, 100, "1. Income categories", "2. Expense categories"))
        {
            scroll.Reset();
            msg.clear();
        }

        DrawCard(Rectangle{40, 165, 820, 430});
        UI::Text("Add category", 70, 175, 20, Theme::TEXT);
        name.Draw();

        bool add = Button(Rectangle{650, 235, 180, 44}, "+ Add", Theme::BTN_GREEN).Frame();
        if (UI::EnterPressed() && name.HasFocus()) add = true;
        DrawMsg(450, 292);

        if (add)
        {
            string err;
            if (ctx.cats.add(type, name.GetText(), err))
            {
                SetMsg("Category added successfully.", true);
                name.Clear();
            }
            else SetMsg(err, false);
        }

        UI::Text(string(type == 'I' ? "Income" : "Expense") + " categories:", 70, 320, 20, Theme::TEXT);

        const vector<string>& cats = ctx.cats.getList(type);
        if (cats.empty())
            UI::TextCentered("(none added yet)", 450, 450, 22, Theme::MUTED);

        Rectangle view = {70, 350, 760, 230};
        float off = scroll.Update(view, cats.size() * 46.0f);
        int removeIdx = -1;

        BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
        for (size_t i = 0; i < cats.size(); i++)
        {
            float y = view.y + i * 46.0f - off;
            DrawRectangleRounded(Rectangle{view.x, y, view.width, 40}, 0.2f, 6,
                                 i % 2 ? Theme::PANEL2 : Color{38, 46, 64, 255});
            UI::Text(to_string(i + 1) + ".  " + cats[i], view.x + 16, y + 10, 20, Theme::TEXT);

            Button b(Rectangle{view.x + view.width - 120, y + 5, 108, 30}, "Remove", Theme::BTN_RED, 18);
            b.SetClip(view);
            if (b.Frame()) removeIdx = (int)i;
        }
        EndScissorMode();

        if (removeIdx >= 0)
        {
            string removed = ctx.cats.remove(type, removeIdx);
            SetMsg("Category \"" + removed + "\" removed.", true);
        }
        BackButton();
    }
};

/* ---------------- Monthly summary ---------------- */
class MonthlySummaryScreen : public Screen
{
private:
    vector<Transaction> list;
    int curM, curY;
    double balance;
    Scroller scroll;

public:
    MonthlySummaryScreen(AppContext& c) : Screen(c), curM(0), curY(0), balance(0) {}

    void OnEnter() override
    {
        InputHelper::getCurrentMonthYear(curM, curY);
        list = ctx.trans.currentMonth();
        balance = 0;
        for (size_t i = 0; i < list.size(); i++) balance += list[i].getAmount();
        scroll.Reset();
    }

    void Frame() override
    {
        DrawHeader("MONTHLY SUMMARY");
        DrawCard(Rectangle{40, 90, 820, 505});

        ostringstream t;
        t << "MONTHLY SUMMARY (" << setfill('0') << setw(2) << curM << "/" << curY << ")";
        UI::TextCentered(t.str(), 450, 105, 28, Theme::TEXT);

        if (list.empty())
        {
            UI::TextCentered("No transactions this month.", 450, 300, 24, Theme::MUTED);
            BackButton();
            return;
        }

        // table header
        DrawRectangleRounded(Rectangle{60, 150, 780, 34}, 0.3f, 6, Theme::ACCENT);
        UI::Text("#", 75, 157, 18, Theme::TEXT);
        UI::Text("Date", 130, 157, 18, Theme::TEXT);
        UI::Text("Category", 300, 157, 18, Theme::TEXT);
        UI::TextRight("Amount", 820, 157, 18, Theme::TEXT);

        Rectangle view = {60, 192, 780, 330};
        float off = scroll.Update(view, list.size() * 36.0f);
        BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
        for (size_t i = 0; i < list.size(); i++)
        {
            float y = view.y + i * 36.0f - off;
            const Transaction& tr = list[i];
            DrawRectangle((int)view.x, (int)y, (int)view.width, 34,
                          i % 2 ? Theme::PANEL2 : Color{38, 46, 64, 255});
            UI::Text(to_string(i + 1) + ".", 75, y + 8, 18, Theme::MUTED);
            UI::Text(InputHelper::dateText(tr.getDay(), tr.getMonth(), tr.getYear()), 130, y + 8, 18, Theme::TEXT);
            UI::Text(tr.getCategory(), 300, y + 8, 18, Theme::TEXT);
            UI::TextRight(InputHelper::money(tr.getAmount()), 820, y + 8, 18,
                          tr.getAmount() >= 0 ? Theme::GOOD : Theme::BAD);
        }
        EndScissorMode();

        DrawRectangle(60, 530, 780, 2, Theme::BORDER);
        UI::TextRight("Balance: " + InputHelper::money(balance), 840, 545, 26,
                      balance >= 0 ? Theme::GOOD : Theme::BAD);
        BackButton();
    }
};

/* =========================================================
   App : creates the window, owns all screens, runs the loop
   ========================================================= */
class App
{
private:
    AppContext ctx;
    vector<unique_ptr<Screen> > screens;
    Screen* current;

public:
    App() : current(0)
    {
        screens.resize(SCR_COUNT);
        screens[SCR_REGISTER].reset(new RegisterScreen(ctx));
        screens[SCR_LOGIN].reset(new LoginScreen(ctx));
        screens[SCR_MENU].reset(new MenuScreen(ctx));
        screens[SCR_ADD].reset(new AddTransactionScreen(ctx));
        screens[SCR_BUDGET].reset(new BudgetScreen(ctx));
        screens[SCR_VIEWCAT].reset(new ViewCategoriesScreen(ctx));
        screens[SCR_EDITCAT].reset(new EditCategoriesScreen(ctx));
        screens[SCR_SUMMARY].reset(new MonthlySummaryScreen(ctx));
    }

    int Run()
    {
        InitWindow(WIN_W, WIN_H, "Personal Expense Tracker");
        SetExitKey(KEY_NULL);          // ESC does not close the program
        SetTargetFPS(60);

        current = screens[ctx.account.exists() ? SCR_LOGIN : SCR_REGISTER].get();
        current->OnEnter();

        while (!ctx.quit && !WindowShouldClose())
        {
            BeginDrawing();
            ClearBackground(Theme::BG);
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            current->Frame();
            EndDrawing();

            if (ctx.nextScreen >= 0)
            {
                current = screens[ctx.nextScreen].get();
                ctx.nextScreen = -1;
                current->OnEnter();
            }
        }
        CloseWindow();
        return 0;
    }
};

int main()
{
    App app;
    return app.Run();
}
