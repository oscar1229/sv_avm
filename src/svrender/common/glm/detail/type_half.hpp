/// @ref core
/// @file glm/detail/type_half.hpp

#ifndef TYPE_HALF_HPP
#define TYPE_HALF_HPP

#pragma once

#include "setup.hpp"

namespace glm{
namespace detail
{
    typedef short hdata;

    GLM_FUNC_DECL float toFloat32(hdata value);
    GLM_FUNC_DECL hdata toFloat16(float const & value);

}//namespace detail
}//namespace glm

#include "type_half.inl"

#endif  // TYPE_HALF_HPP
