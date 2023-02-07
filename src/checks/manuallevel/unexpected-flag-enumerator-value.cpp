/*
  This file is part of the clazy static checker.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Waqar Ahmed <waqar.ahmed@kdab.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "unexpected-flag-enumerator-value.h"
#include "HierarchyUtils.h"

#include <clang/AST/AST.h>

#include <algorithm>

using namespace clang;

UnexpectedFlagEnumeratorValue::UnexpectedFlagEnumeratorValue(const std::string &name, ClazyContext *context)
    : CheckBase(name, context)
{
}

static ConstantExpr *getConstantExpr(EnumConstantDecl *enCD)
{
    auto cexpr = dyn_cast_or_null<ConstantExpr>(enCD->getInitExpr());
    if (cexpr)
        return cexpr;
    return clazy::getFirstChildOfType<ConstantExpr>(enCD->getInitExpr());
}

static bool isBinaryOperatorExpression(ConstantExpr *cexpr)
{
    return clazy::getFirstChildOfType<BinaryOperator>(cexpr);
}

static bool isReferenceToEnumerator(ConstantExpr *cexpr)
{
    return dyn_cast_or_null<DeclRefExpr>(cexpr->getSubExpr());
}

static bool isIntentionallyNotPowerOf2(EnumConstantDecl *en)
{
    constexpr unsigned MinOnesToQualifyAsMask = 3;

    const auto val = en->getInitVal();
    if (val.isMask() && val.countTrailingOnes() >= MinOnesToQualifyAsMask)
        return true;

    if (val.isShiftedMask() && val.countPopulation() >= MinOnesToQualifyAsMask)
        return true;

    if (clazy::contains_lower(en->getName(), "mask"))
        return true;

    auto *cexpr = getConstantExpr(en);
    if (!cexpr)
        return false;

    if (isBinaryOperatorExpression(cexpr))
        return true;

    if (isReferenceToEnumerator(cexpr))
        return true;

    return false;
}

static SmallVector<EnumConstantDecl *, 16> getEnumerators(EnumDecl *enDecl)
{
    SmallVector<EnumConstantDecl *, 16> ret;
    for (auto *enumerator : enDecl->enumerators()) {
        ret.push_back(enumerator);
    }
    return ret;
}

static uint64_t getIntegerValue(EnumConstantDecl *e)
{
    return e->getInitVal().getLimitedValue();
}

static bool hasConsecutiveValues(const SmallVector<EnumConstantDecl *, 16> &enumerators)
{
    auto val = getIntegerValue(enumerators.front());
    const size_t until = std::min<size_t>(4, enumerators.size());
    for (size_t i = 1; i < until; ++i) {
        val++;
        if (getIntegerValue(enumerators[i]) != val)
            return false;
    }
    return true;
}

static bool hasInitExprs(const SmallVector<EnumConstantDecl *, 16> &enumerators)
{
    size_t enumeratorsWithInitExpr = 0;
    for (auto enumerator : enumerators) {
        if (enumerator->getInitExpr()) {
            enumeratorsWithInitExpr++;
        }
    }

    return enumeratorsWithInitExpr == enumerators.size();
}

static bool isFlagEnum(const SmallVector<EnumConstantDecl *, 16> &enumerators)
{
    if (enumerators.size() < 4) {
        return false;
    }

    // For an enum to be considered "flag like", all enumerators
    // must have an explicit init value / expr
    if (!hasInitExprs(enumerators)) {
        return false;
    }

    if (hasConsecutiveValues(enumerators)) {
        return false;
    }

    llvm::SmallVector<bool, 16> enumValues;
    for (auto *enumerator : enumerators) {
        enumValues.push_back(enumerator->getInitVal().isPowerOf2());
    }

    const size_t count = std::count(enumValues.begin(), enumValues.end(), false);

    // If half of our values were power-of-2, this is probably a flag enum
    return count <= (enumerators.size() / 2);
}

void UnexpectedFlagEnumeratorValue::VisitDecl(clang::Decl *decl)
{
    auto enDecl = dyn_cast_or_null<EnumDecl>(decl);
    if (!enDecl || !enDecl->hasNameForLinkage())
        return;

    const SmallVector<EnumConstantDecl *, 16> enumerators = getEnumerators(enDecl);

    if (!isFlagEnum(enumerators))
        return;

    for (EnumConstantDecl *enumerator : enumerators) {
        const auto &initVal = enumerator->getInitVal();
        if (!initVal.isPowerOf2() && !initVal.isNullValue() && !initVal.isNegative()) {
            if (isIntentionallyNotPowerOf2(enumerator))
                continue;
            const auto value = enumerator->getInitVal().getLimitedValue();
            Expr *initExpr = enumerator->getInitExpr();
            emitWarning(initExpr ? initExpr->getBeginLoc() : enumerator->getBeginLoc(), "Unexpected non power-of-2 enumerator value: " + std::to_string(value));
        }
    }
}
