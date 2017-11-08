/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/Log.h>

#include "SkFontMgr.h"
#include "SkStream.h"

#include "hwui/MinikinSkia.h"
#include "hwui/Typeface.h"

using namespace android;

namespace {

constexpr char kRobotoRegular[] = "/system/fonts/Roboto-Regular.ttf";
constexpr char kRobotoBold[] = "/system/fonts/Roboto-Bold.ttf";
constexpr char kRobotoItalic[] = "/system/fonts/Roboto-Italic.ttf";
constexpr char kRobotoBoldItalic[] = "/system/fonts/Roboto-BoldItalic.ttf";

void unmap(const void* ptr, void* context) {
    void* p = const_cast<void*>(ptr);
    size_t len = reinterpret_cast<size_t>(context);
    munmap(p, len);
}

std::shared_ptr<minikin::FontFamily> buildFamily(const char* fileName) {
    int fd = open(fileName, O_RDONLY);
    LOG_ALWAYS_FATAL_IF(fd == -1, "Failed to open file %s", fileName);
    struct stat st = {};
    LOG_ALWAYS_FATAL_IF(fstat(fd, &st) == -1, "Failed to stat file %s", fileName);
    void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    sk_sp<SkData> skData =
            SkData::MakeWithProc(data, st.st_size, unmap, reinterpret_cast<void*>(st.st_size));
    std::unique_ptr<SkStreamAsset> fontData(new SkMemoryStream(skData));
    sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
    sk_sp<SkTypeface> typeface(fm->makeFromStream(std::move(fontData)));
    LOG_ALWAYS_FATAL_IF(typeface == nullptr, "Failed to make typeface from %s", fileName);
    std::shared_ptr<minikin::MinikinFont> font = std::make_shared<MinikinFontSkia>(
            std::move(typeface), data, st.st_size, 0, std::vector<minikin::FontVariation>());
    return std::make_shared<minikin::FontFamily>(
            std::vector<minikin::Font>({minikin::Font(std::move(font), minikin::FontStyle())}));
}

std::vector<std::shared_ptr<minikin::FontFamily>> makeSingleFamlyVector(const char* fileName) {
    return std::vector<std::shared_ptr<minikin::FontFamily>>({buildFamily(fileName)});
}

TEST(TypefaceTest, resolveDefault_and_setDefaultTest) {
    std::unique_ptr<Typeface> regular(Typeface::createFromFamilies(
            makeSingleFamlyVector(kRobotoRegular), RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE));
    EXPECT_EQ(regular.get(), Typeface::resolveDefault(regular.get()));

    // Keep the original to restore it later.
    const Typeface* old = Typeface::resolveDefault(nullptr);
    ASSERT_NE(nullptr, old);

    Typeface::setDefault(regular.get());
    EXPECT_EQ(regular.get(), Typeface::resolveDefault(nullptr));

    Typeface::setDefault(old);  // Restore to the original.
}

TEST(TypefaceTest, createWithDifferentBaseWeight) {
    std::unique_ptr<Typeface> bold(Typeface::createWithDifferentBaseWeight(nullptr, 700));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, bold->fAPIStyle);

    std::unique_ptr<Typeface> light(Typeface::createWithDifferentBaseWeight(nullptr, 300));
    EXPECT_EQ(300, light->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, light->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, light->fAPIStyle);
}

TEST(TypefaceTest, createRelativeTest_fromRegular) {
    // In Java, Typeface.create(Typeface.DEFAULT, Typeface.NORMAL);
    std::unique_ptr<Typeface> normal(Typeface::createRelative(nullptr, Typeface::kNormal));
    EXPECT_EQ(400, normal->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, normal->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, normal->fAPIStyle);

    // In Java, Typeface.create(Typeface.DEFAULT, Typeface.BOLD);
    std::unique_ptr<Typeface> bold(Typeface::createRelative(nullptr, Typeface::kBold));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java, Typeface.create(Typeface.DEFAULT, Typeface.ITALIC);
    std::unique_ptr<Typeface> italic(Typeface::createRelative(nullptr, Typeface::kItalic));
    EXPECT_EQ(400, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java, Typeface.create(Typeface.DEFAULT, Typeface.BOLD_ITALIC);
    std::unique_ptr<Typeface> boldItalic(Typeface::createRelative(nullptr, Typeface::kBoldItalic));
    EXPECT_EQ(700, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kBoldItalic, boldItalic->fAPIStyle);
}

TEST(TypefaceTest, createRelativeTest_BoldBase) {
    std::unique_ptr<Typeface> base(Typeface::createWithDifferentBaseWeight(nullptr, 700));

    // In Java, Typeface.create(Typeface.create("sans-serif-bold"),
    // Typeface.NORMAL);
    std::unique_ptr<Typeface> normal(Typeface::createRelative(base.get(), Typeface::kNormal));
    EXPECT_EQ(700, normal->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, normal->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, normal->fAPIStyle);

    // In Java, Typeface.create(Typeface.create("sans-serif-bold"),
    // Typeface.BOLD);
    std::unique_ptr<Typeface> bold(Typeface::createRelative(base.get(), Typeface::kBold));
    EXPECT_EQ(1000, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java, Typeface.create(Typeface.create("sans-serif-bold"),
    // Typeface.ITALIC);
    std::unique_ptr<Typeface> italic(Typeface::createRelative(base.get(), Typeface::kItalic));
    EXPECT_EQ(700, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java, Typeface.create(Typeface.create("sans-serif-bold"),
    // Typeface.BOLD_ITALIC);
    std::unique_ptr<Typeface> boldItalic(
            Typeface::createRelative(base.get(), Typeface::kBoldItalic));
    EXPECT_EQ(1000, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kBoldItalic, boldItalic->fAPIStyle);
}

TEST(TypefaceTest, createRelativeTest_LightBase) {
    std::unique_ptr<Typeface> base(Typeface::createWithDifferentBaseWeight(nullptr, 300));

    // In Java, Typeface.create(Typeface.create("sans-serif-light"),
    // Typeface.NORMAL);
    std::unique_ptr<Typeface> normal(Typeface::createRelative(base.get(), Typeface::kNormal));
    EXPECT_EQ(300, normal->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, normal->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, normal->fAPIStyle);

    // In Java, Typeface.create(Typeface.create("sans-serif-light"),
    // Typeface.BOLD);
    std::unique_ptr<Typeface> bold(Typeface::createRelative(base.get(), Typeface::kBold));
    EXPECT_EQ(600, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java, Typeface.create(Typeface.create("sans-serif-light"),
    // Typeface.ITLIC);
    std::unique_ptr<Typeface> italic(Typeface::createRelative(base.get(), Typeface::kItalic));
    EXPECT_EQ(300, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java, Typeface.create(Typeface.create("sans-serif-light"),
    // Typeface.BOLD_ITALIC);
    std::unique_ptr<Typeface> boldItalic(
            Typeface::createRelative(base.get(), Typeface::kBoldItalic));
    EXPECT_EQ(600, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kBoldItalic, boldItalic->fAPIStyle);
}

TEST(TypefaceTest, createRelativeTest_fromBoldStyled) {
    std::unique_ptr<Typeface> base(Typeface::createRelative(nullptr, Typeface::kBold));

    // In Java, Typeface.create(Typeface.create(Typeface.DEFAULT, Typeface.BOLD),
    // Typeface.NORMAL);
    std::unique_ptr<Typeface> normal(Typeface::createRelative(base.get(), Typeface::kNormal));
    EXPECT_EQ(400, normal->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, normal->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, normal->fAPIStyle);

    // In Java Typeface.create(Typeface.create(Typeface.DEFAULT, Typeface.BOLD),
    // Typeface.BOLD);
    std::unique_ptr<Typeface> bold(Typeface::createRelative(base.get(), Typeface::kBold));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java, Typeface.create(Typeface.create(Typeface.DEFAULT, Typeface.BOLD),
    // Typeface.ITALIC);
    std::unique_ptr<Typeface> italic(Typeface::createRelative(base.get(), Typeface::kItalic));
    EXPECT_EQ(400, normal->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java,
    // Typeface.create(Typeface.create(Typeface.DEFAULT, Typeface.BOLD),
    // Typeface.BOLD_ITALIC);
    std::unique_ptr<Typeface> boldItalic(
            Typeface::createRelative(base.get(), Typeface::kBoldItalic));
    EXPECT_EQ(700, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kBoldItalic, boldItalic->fAPIStyle);
}

TEST(TypefaceTest, createRelativeTest_fromItalicStyled) {
    std::unique_ptr<Typeface> base(Typeface::createRelative(nullptr, Typeface::kItalic));

    // In Java,
    // Typeface.create(Typeface.create(Typeface.DEFAULT, Typeface.ITALIC),
    // Typeface.NORMAL);
    std::unique_ptr<Typeface> normal(Typeface::createRelative(base.get(), Typeface::kNormal));
    EXPECT_EQ(400, normal->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, normal->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, normal->fAPIStyle);

    // In Java, Typeface.create(Typeface.create(Typeface.DEFAULT,
    // Typeface.ITALIC), Typeface.BOLD);
    std::unique_ptr<Typeface> bold(Typeface::createRelative(base.get(), Typeface::kBold));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java,
    // Typeface.create(Typeface.create(Typeface.DEFAULT, Typeface.ITALIC),
    // Typeface.ITALIC);
    std::unique_ptr<Typeface> italic(Typeface::createRelative(base.get(), Typeface::kItalic));
    EXPECT_EQ(400, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java,
    // Typeface.create(Typeface.create(Typeface.DEFAULT, Typeface.ITALIC),
    // Typeface.BOLD_ITALIC);
    std::unique_ptr<Typeface> boldItalic(
            Typeface::createRelative(base.get(), Typeface::kBoldItalic));
    EXPECT_EQ(700, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kBoldItalic, boldItalic->fAPIStyle);
}

TEST(TypefaceTest, createRelativeTest_fromSpecifiedStyled) {
    std::unique_ptr<Typeface> base(Typeface::createAbsolute(nullptr, 400, false));

    // In Java,
    // Typeface typeface = new Typeface.Builder(invalid).setFallback("sans-serif")
    //     .setWeight(700).setItalic(false).build();
    // Typeface.create(typeface, Typeface.NORMAL);
    std::unique_ptr<Typeface> normal(Typeface::createRelative(base.get(), Typeface::kNormal));
    EXPECT_EQ(400, normal->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, normal->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, normal->fAPIStyle);

    // In Java,
    // Typeface typeface = new Typeface.Builder(invalid).setFallback("sans-serif")
    //     .setWeight(700).setItalic(false).build();
    // Typeface.create(typeface, Typeface.BOLD);
    std::unique_ptr<Typeface> bold(Typeface::createRelative(base.get(), Typeface::kBold));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java,
    // Typeface typeface = new Typeface.Builder(invalid).setFallback("sans-serif")
    //     .setWeight(700).setItalic(false).build();
    // Typeface.create(typeface, Typeface.ITALIC);
    std::unique_ptr<Typeface> italic(Typeface::createRelative(base.get(), Typeface::kItalic));
    EXPECT_EQ(400, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java,
    // Typeface typeface = new Typeface.Builder(invalid).setFallback("sans-serif")
    //     .setWeight(700).setItalic(false).build();
    // Typeface.create(typeface, Typeface.BOLD_ITALIC);
    std::unique_ptr<Typeface> boldItalic(
            Typeface::createRelative(base.get(), Typeface::kBoldItalic));
    EXPECT_EQ(700, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kBoldItalic, boldItalic->fAPIStyle);
}

TEST(TypefaceTest, createAbsolute) {
    // In Java,
    // new
    // Typeface.Builder(invalid).setFallback("sans-serif").setWeight(400).setItalic(false)
    //     .build();
    std::unique_ptr<Typeface> regular(Typeface::createAbsolute(nullptr, 400, false));
    EXPECT_EQ(400, regular->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, regular->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, regular->fAPIStyle);

    // In Java,
    // new
    // Typeface.Builder(invalid).setFallback("sans-serif").setWeight(700).setItalic(false)
    //     .build();
    std::unique_ptr<Typeface> bold(Typeface::createAbsolute(nullptr, 700, false));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java,
    // new
    // Typeface.Builder(invalid).setFallback("sans-serif").setWeight(400).setItalic(true)
    //     .build();
    std::unique_ptr<Typeface> italic(Typeface::createAbsolute(nullptr, 400, true));
    EXPECT_EQ(400, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java,
    // new
    // Typeface.Builder(invalid).setFallback("sans-serif").setWeight(700).setItalic(true)
    //     .build();
    std::unique_ptr<Typeface> boldItalic(Typeface::createAbsolute(nullptr, 700, true));
    EXPECT_EQ(700, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kBoldItalic, boldItalic->fAPIStyle);

    // In Java,
    // new
    // Typeface.Builder(invalid).setFallback("sans-serif").setWeight(1100).setItalic(true)
    //     .build();
    std::unique_ptr<Typeface> over1000(Typeface::createAbsolute(nullptr, 1100, false));
    EXPECT_EQ(1000, over1000->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, over1000->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, over1000->fAPIStyle);
}

TEST(TypefaceTest, createFromFamilies_Single) {
    // In Java, new
    // Typeface.Builder("Roboto-Regular.ttf").setWeight(400).setItalic(false).build();
    std::unique_ptr<Typeface> regular(
            Typeface::createFromFamilies(makeSingleFamlyVector(kRobotoRegular), 400, false));
    EXPECT_EQ(400, regular->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, regular->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, regular->fAPIStyle);

    // In Java, new
    // Typeface.Builder("Roboto-Bold.ttf").setWeight(700).setItalic(false).build();
    std::unique_ptr<Typeface> bold(
            Typeface::createFromFamilies(makeSingleFamlyVector(kRobotoBold), 700, false));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java, new
    // Typeface.Builder("Roboto-Italic.ttf").setWeight(400).setItalic(true).build();
    std::unique_ptr<Typeface> italic(
            Typeface::createFromFamilies(makeSingleFamlyVector(kRobotoItalic), 400, true));
    EXPECT_EQ(400, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java,
    // new
    // Typeface.Builder("Roboto-BoldItalic.ttf").setWeight(700).setItalic(true).build();
    std::unique_ptr<Typeface> boldItalic(
            Typeface::createFromFamilies(makeSingleFamlyVector(kRobotoBoldItalic), 700, true));
    EXPECT_EQ(700, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java,
    // new
    // Typeface.Builder("Roboto-BoldItalic.ttf").setWeight(1100).setItalic(false).build();
    std::unique_ptr<Typeface> over1000(
            Typeface::createFromFamilies(makeSingleFamlyVector(kRobotoBold), 1100, false));
    EXPECT_EQ(1000, over1000->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, over1000->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, over1000->fAPIStyle);
}

TEST(TypefaceTest, createFromFamilies_Single_resolveByTable) {
    // In Java, new Typeface.Builder("Roboto-Regular.ttf").build();
    std::unique_ptr<Typeface> regular(Typeface::createFromFamilies(
            makeSingleFamlyVector(kRobotoRegular), RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE));
    EXPECT_EQ(400, regular->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, regular->fStyle.slant);
    EXPECT_EQ(Typeface::kNormal, regular->fAPIStyle);

    // In Java, new Typeface.Builder("Roboto-Bold.ttf").build();
    std::unique_ptr<Typeface> bold(Typeface::createFromFamilies(
            makeSingleFamlyVector(kRobotoBold), RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE));
    EXPECT_EQ(700, bold->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, bold->fStyle.slant);
    EXPECT_EQ(Typeface::kBold, bold->fAPIStyle);

    // In Java, new Typeface.Builder("Roboto-Italic.ttf").build();
    std::unique_ptr<Typeface> italic(Typeface::createFromFamilies(
            makeSingleFamlyVector(kRobotoItalic), RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE));
    EXPECT_EQ(400, italic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, italic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);

    // In Java, new Typeface.Builder("Roboto-BoldItalic.ttf").build();
    std::unique_ptr<Typeface> boldItalic(
            Typeface::createFromFamilies(makeSingleFamlyVector(kRobotoBoldItalic),
                                         RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE));
    EXPECT_EQ(700, boldItalic->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::ITALIC, boldItalic->fStyle.slant);
    EXPECT_EQ(Typeface::kItalic, italic->fAPIStyle);
}

TEST(TypefaceTest, createFromFamilies_Family) {
    std::vector<std::shared_ptr<minikin::FontFamily>> families = {
            buildFamily(kRobotoRegular), buildFamily(kRobotoBold), buildFamily(kRobotoItalic),
            buildFamily(kRobotoBoldItalic)};
    std::unique_ptr<Typeface> typeface(Typeface::createFromFamilies(
            std::move(families), RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE));
    EXPECT_EQ(400, typeface->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, typeface->fStyle.slant);
}

TEST(TypefaceTest, createFromFamilies_Family_withoutRegular) {
    std::vector<std::shared_ptr<minikin::FontFamily>> families = {
            buildFamily(kRobotoBold), buildFamily(kRobotoItalic), buildFamily(kRobotoBoldItalic)};
    std::unique_ptr<Typeface> typeface(Typeface::createFromFamilies(
            std::move(families), RESOLVE_BY_FONT_TABLE, RESOLVE_BY_FONT_TABLE));
    EXPECT_EQ(700, typeface->fStyle.weight);
    EXPECT_EQ(minikin::FontSlant::UPRIGHT, typeface->fStyle.slant);
}

}  // namespace
