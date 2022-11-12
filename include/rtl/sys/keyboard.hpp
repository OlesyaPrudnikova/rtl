/*
 * Copyright (C) 2016-2022 Konstantin Polevik
 * All rights reserved
 *
 * This file is part of the RTL library. Redistribution and use in source and
 * binary forms, with or without modification, are permitted exclusively
 * under the terms of the MIT license. You should have received a copy of the
 * license with this file. If not, please visit:
 * https://github.com/out61h/rtl/blob/main/LICENSE.
 */
#pragma once

namespace rtl
{
    namespace keyboard
    {
        // TODO: define and handle more keys
        enum class keys
        {
            any,
            enter,
            esc,
            space,
            count
        };
    } // namespace keyboard
} // namespace rtl