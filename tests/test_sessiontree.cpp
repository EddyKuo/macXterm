#include "core/SessionTree.h"
#include <QtTest/QtTest>

using namespace macxterm::core;

static Session mk(const QString& name, const QString& folder, SessionType t = SessionType::Ssh) {
    Session s(name, t);
    if (!folder.isEmpty()) s.setParam("folder", folder);
    return s;
}

class TestSessionTree : public QObject {
    Q_OBJECT
private slots:
    // Loose sessions go first (empty label), then named folders in first-seen
    // order; session order within each group is preserved.
    void groupsLooseThenFolders() {
        QList<Session> in = {
            mk("a", ""), mk("b", "Prod"), mk("c", ""), mk("d", "Dev"), mk("e", "Prod"),
        };
        const QList<FolderGroup> g = groupSessionsByFolder(in);
        QCOMPARE(g.size(), 3);
        QCOMPARE(g[0].folder, QString());              // loose bucket
        QCOMPARE(g[0].sessions.size(), 2);             // a, c
        QCOMPARE(g[0].sessions[0].name(), QStringLiteral("a"));
        QCOMPARE(g[0].sessions[1].name(), QStringLiteral("c"));
        QCOMPARE(g[1].folder, QStringLiteral("Prod")); // first folder seen
        QCOMPARE(g[1].sessions.size(), 2);             // b, e
        QCOMPARE(g[2].folder, QStringLiteral("Dev"));
        QCOMPARE(g[2].sessions.size(), 1);
    }

    // An all-foldered list has no stray empty loose bucket.
    void noEmptyLooseBucket() {
        QList<Session> in = { mk("a", "X"), mk("b", "X") };
        const QList<FolderGroup> g = groupSessionsByFolder(in);
        QCOMPARE(g.size(), 1);
        QCOMPARE(g[0].folder, QStringLiteral("X"));
    }

    void emptyInputYieldsNoGroups() {
        QVERIFY(groupSessionsByFolder({}).isEmpty());
    }

    void folderNamesAreDistinctFirstSeen() {
        QList<Session> in = { mk("a", "B"), mk("b", "A"), mk("c", "B"), mk("d", "") };
        QCOMPARE(folderNames(in), (QStringList{QStringLiteral("B"), QStringLiteral("A")}));
    }

    // The live tree filter matches name/host/username/folder, case-insensitively;
    // a blank query matches everything.
    void filterMatchesAcrossFields() {
        Session s("WebProd", SessionType::Ssh);
        s.setParam("host", "db.example.com");
        s.setParam("username", "deploy");
        s.setParam("folder", "Production");

        QVERIFY(sessionMatchesFilter(s, QString()));            // blank → all
        QVERIFY(sessionMatchesFilter(s, QStringLiteral("  ")));  // whitespace → all
        QVERIFY(sessionMatchesFilter(s, QStringLiteral("web")));      // name, case-insensitive
        QVERIFY(sessionMatchesFilter(s, QStringLiteral("EXAMPLE")));  // host
        QVERIFY(sessionMatchesFilter(s, QStringLiteral("deploy")));   // username
        QVERIFY(sessionMatchesFilter(s, QStringLiteral("prod")));     // folder (and name substring)
        QVERIFY(!sessionMatchesFilter(s, QStringLiteral("staging"))); // no field matches
    }

    // The pure edit operations that back the tree's right-click context menu.
    void renameSessionRejectsBlankAndCollision() {
        QList<Session> list = { mk("alpha", ""), mk("beta", "") };
        QVERIFY(!renameSessionInList(list, "alpha", "   "));     // blank rejected
        QVERIFY(!renameSessionInList(list, "alpha", "beta"));    // collision rejected
        QVERIFY(!renameSessionInList(list, "ghost", "x"));       // missing source
        QCOMPARE(list[0].name(), QStringLiteral("alpha"));       // unchanged on failure
        QVERIFY(renameSessionInList(list, "alpha", "alpha"));    // same-name no-op ok
        QVERIFY(renameSessionInList(list, "alpha", " gamma "));  // trimmed + applied
        QCOMPARE(list[0].name(), QStringLiteral("gamma"));
    }

    void moveSessionAndIconEditFolderParam() {
        QList<Session> list = { mk("a", "Old") };
        QVERIFY(moveSessionToFolder(list, "a", "  New  "));      // trimmed
        QCOMPARE(list[0].param("folder"), QStringLiteral("New"));
        QVERIFY(moveSessionToFolder(list, "a", ""));             // empty = to root
        QVERIFY(list[0].param("folder").isEmpty());
        QVERIFY(!moveSessionToFolder(list, "ghost", "X"));

        QVERIFY(setSessionIcon(list, "a", "⭐"));
        QCOMPARE(sessionGlyph(list[0]), QStringLiteral("⭐"));
        QVERIFY(setSessionIcon(list, "a", ""));                  // cleared → type default
        QCOMPARE(sessionGlyph(list[0]), QStringLiteral("🔑"));
        QVERIFY(!setSessionIcon(list, "ghost", "x"));
    }

    void renameFolderMovesAllMembers() {
        QList<Session> list = { mk("a", "Prod"), mk("b", "Prod"), mk("c", "Dev") };
        QCOMPARE(renameFolderInList(list, "Prod", "Production"), 2);
        QCOMPARE(list[0].param("folder"), QStringLiteral("Production"));
        QCOMPARE(list[1].param("folder"), QStringLiteral("Production"));
        QCOMPARE(list[2].param("folder"), QStringLiteral("Dev"));   // untouched
        QCOMPARE(renameFolderInList(list, "Nope", "X"), 0);         // no members
    }

    void uniqueCopyNameSkipsTaken() {
        QList<Session> list = { mk("host", ""), mk("host (copy)", "") };
        QCOMPARE(uniqueCopyName(list, "host"), QStringLiteral("host (copy 2)"));
        list.append(mk("host (copy 2)", ""));
        QCOMPARE(uniqueCopyName(list, "host"), QStringLiteral("host (copy 3)"));
        QCOMPARE(uniqueCopyName(list, "fresh"), QStringLiteral("fresh (copy)"));
    }

    // Explicit icon wins; otherwise the glyph defaults from the session type.
    void glyphPrefersExplicitIconThenType() {
        Session custom("x", SessionType::Ssh);
        custom.setParam("icon", "⭐");
        QCOMPARE(sessionGlyph(custom), QStringLiteral("⭐"));
        QCOMPARE(sessionGlyph(Session("y", SessionType::Ssh)), QStringLiteral("🔑"));
        QCOMPARE(sessionGlyph(Session("z", SessionType::Vnc)), QStringLiteral("👁️"));
        QVERIFY(!sessionGlyph(Session("q", SessionType::Unknown)).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestSessionTree)
#include "test_sessiontree.moc"
