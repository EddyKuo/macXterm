#include "tools/FolderDiff.h"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

using namespace macxterm::tools;

class TestFolderDiff : public QObject {
    Q_OBJECT
private:
    static void writeFile(const QString& path, const QByteArray& data) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    static FolderDiffEntry::Kind kindOf(const QList<FolderDiffEntry>& e, const QString& rel) {
        for (const auto& x : e) if (x.relPath == rel) return x.kind;
        return FolderDiffEntry::Same;   // not found sentinel; tests assert presence separately
    }
    static bool has(const QList<FolderDiffEntry>& e, const QString& rel) {
        for (const auto& x : e) if (x.relPath == rel) return true;
        return false;
    }

private slots:
    void classifiesEntries() {
        QTemporaryDir a, b;
        QVERIFY(a.isValid() && b.isValid());
        writeFile(a.filePath("same.txt"), "hello");
        writeFile(b.filePath("same.txt"), "hello");
        writeFile(a.filePath("diff.txt"), "aaa");
        writeFile(b.filePath("diff.txt"), "bbb");
        writeFile(a.filePath("onlyA.txt"), "x");
        writeFile(b.filePath("onlyB.txt"), "y");
        writeFile(a.filePath("sub/nested.txt"), "n");
        writeFile(b.filePath("sub/nested.txt"), "n");

        const auto e = FolderDiff::compare(a.path(), b.path());
        QVERIFY(has(e, "same.txt"));
        QCOMPARE(kindOf(e, "same.txt"), FolderDiffEntry::Same);
        QCOMPARE(kindOf(e, "diff.txt"), FolderDiffEntry::Differ);
        QCOMPARE(kindOf(e, "onlyA.txt"), FolderDiffEntry::OnlyLeft);
        QCOMPARE(kindOf(e, "onlyB.txt"), FolderDiffEntry::OnlyRight);
        QCOMPARE(kindOf(e, "sub/nested.txt"), FolderDiffEntry::Same);   // recursive
    }

    void differentSizesAreDiff() {
        QTemporaryDir a, b;
        writeFile(a.filePath("f"), "short");
        writeFile(b.filePath("f"), "a much longer content");
        const auto e = FolderDiff::compare(a.path(), b.path());
        QCOMPARE(kindOf(e, "f"), FolderDiffEntry::Differ);
    }
};

QTEST_MAIN(TestFolderDiff)
#include "test_folderdiff.moc"
