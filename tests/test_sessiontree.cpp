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
