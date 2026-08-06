#include "index_v2/delta_overlay.h"
#include "index_v2/snapshot.h"

#include <gtest/gtest.h>

using namespace swiftlist::index_v2;

TEST(DeltaOverlayTest, RemoveBaseRowPopulatesOverridesAndDeleted) {
    DeltaOverlay overlay;
    UInt128 id1{1, 0};
    overlay.Remove(id1, 0);

    auto overrides = overlay.Overrides();
    ASSERT_EQ(overrides.size(), 1);
    EXPECT_TRUE(overrides[0].Removed);
    EXPECT_EQ(overrides[0].Id.low, 1);

    auto deleted = overlay.DeletedBaseRows();
    ASSERT_EQ(deleted.size(), 1);
    EXPECT_TRUE(deleted.count(0) > 0);
}

TEST(DeltaOverlayTest, RemoveMultipleBaseRowsNoOverwrite) {
    DeltaOverlay overlay;
    overlay.Remove(UInt128{1, 0}, 0);
    overlay.Remove(UInt128{2, 0}, 5);

    auto overrides = overlay.Overrides();
    ASSERT_EQ(overrides.size(), 2);
    EXPECT_TRUE(overrides[0].Removed);
    EXPECT_TRUE(overrides[5].Removed);

    auto deleted = overlay.DeletedBaseRows();
    ASSERT_EQ(deleted.size(), 2);
    EXPECT_TRUE(deleted.count(0) > 0);
    EXPECT_TRUE(deleted.count(5) > 0);
}

TEST(DeltaOverlayTest, RemoveUnknownIdNoBaseRowDoesNothing) {
    DeltaOverlay overlay;
    overlay.Remove(UInt128{1, 0}, -1);

    EXPECT_TRUE(overlay.Overrides().empty());
    EXPECT_TRUE(overlay.DeletedBaseRows().empty());
    EXPECT_TRUE(overlay.IsEmpty());
}

TEST(DeltaOverlayTest, RemoveFromAddedList) {
    DeltaOverlay overlay;
    FileRecordInput input;
    input.Id = UInt128{1, 0};
    input.Name = "test.txt";
    overlay.Upsert(std::move(input));

    // Remove from added (no base row since it's not in snapshot).
    overlay.Remove(UInt128{1, 0}, -1);

    EXPECT_TRUE(overlay.IsEmpty());
    EXPECT_FALSE(overlay.Contains(UInt128{1, 0}));
}

TEST(DeltaOverlayTest, RemoveThenClearResetsAll) {
    DeltaOverlay overlay;
    overlay.Remove(UInt128{1, 0}, 0);
    overlay.Remove(UInt128{2, 0}, 1);

    overlay.Clear();

    EXPECT_TRUE(overlay.IsEmpty());
    EXPECT_TRUE(overlay.Overrides().empty());
    EXPECT_TRUE(overlay.DeletedBaseRows().empty());
}