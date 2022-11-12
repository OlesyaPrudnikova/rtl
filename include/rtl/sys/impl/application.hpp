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

#ifndef RTL_IMPLEMENTATION
    #error "Do not include implementation header directly, use <rtl/sys/impl.hpp>"
#endif

#include <rtl/algorithm.hpp>
#include <rtl/sys/application.hpp>
#include <rtl/sys/debug.hpp>

#include "keyboard.hpp"
#include "memory.hpp"
#include "win.hpp"

#if RTL_ENABLE_APP

namespace rtl
{
    namespace impl
    {
        namespace win
        {
            class window final
            {
            public:
                void create( const wchar_t* window_name );
                void update( application::reset_function*  on_resize,
                             application::update_function* on_update );
                void destroy();

                int  width() const;
                int  height() const;
                bool fullscreen() const;

            private:
                void destroy_resizable_components();
                void resize();
                void invalidate();

    #if RTL_ENABLE_APP_RESIZE
                void set_fullscreen_mode( bool fullscreen );
    #endif

    #if RTL_ENABLE_APP_OSD
                void init_osd_text( int width, int height );
                void draw_osd_text();
    #endif

                static constexpr int  minimal_width = 600;
                static constexpr int  minimal_height = 400;
                static constexpr bool is_fullscreen = RTL_ENABLE_APP_FULLSCREEN;
                static constexpr bool is_resizable = RTL_ENABLE_APP_RESIZE;
                static constexpr bool has_cursor = RTL_ENABLE_APP_CURSOR;

                static constexpr DWORD fullscreen_style = WS_POPUP;
                static constexpr DWORD resizable_style = WS_OVERLAPPEDWINDOW;
                static constexpr DWORD fixed_style
                    = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;

                static LRESULT CALLBACK wnd_proc( HWND   hWnd,
                                                  UINT   uMsg,
                                                  WPARAM wParam,
                                                  LPARAM lParam );

                // NOTE: all variables must be initialized to zero
                WNDCLASSW m_window_class{ 0 };

                HDC  m_device_context_handle{ nullptr };
                HWND m_window_handle{ nullptr };
                RECT m_client_rect{ 0 };

                application::input  m_input{ 0 };
                application::output m_output{ 0 };

    #if RTL_ENABLE_APP_RESIZE
                bool m_sizing{ false };
                bool m_sized{ false };
                bool m_fullscreen{ false };
                bool m_pad{ false };

                WINDOWPLACEMENT m_placement;
    #endif

    #if RTL_ENABLE_APP_SCREEN
                BITMAPINFO m_bitmap_info{ 0 };
                HBITMAP    m_bitmap_handle{ nullptr };
    #endif

    #if RTL_ENABLE_APP_OSD
                static constexpr auto osd_locations_count
                    = (size_t)application::output::osd::location::count;

                RECT  m_osd_rects[osd_locations_count]{ 0 };
                UINT  m_osd_params[osd_locations_count]{ 0 };
                HFONT m_osd_font{ nullptr };
    #endif
            };

            int window::width() const
            {
                return m_client_rect.right - m_client_rect.left;
            }

            int window::height() const
            {
                return m_client_rect.bottom - m_client_rect.top;
            }

            bool window::fullscreen() const
            {
    #if RTL_ENABLE_APP_RESIZE
                return m_fullscreen;
    #else
                return RTL_ENABLE_APP_FULLSCREEN;
    #endif
            }

            void window::create( const wchar_t* window_name )
            {
                m_window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
                m_window_class.lpfnWndProc = wnd_proc;
                m_window_class.lpszClassName = L".";

                m_window_class.hInstance = ::GetModuleHandleW( nullptr );
                RTL_WINAPI_CHECK( m_window_class.hInstance != nullptr );

                m_window_class.hbrBackground = ( HBRUSH )::GetStockObject( BLACK_BRUSH );
                RTL_WINAPI_CHECK( m_window_class.hbrBackground != nullptr );

                if constexpr ( has_cursor || is_resizable )
                {
                    m_window_class.hCursor = ::LoadCursorW( nullptr, IDC_ARROW );
                    RTL_WINAPI_CHECK( m_window_class.hCursor != nullptr );
                }

                [[maybe_unused]] ATOM atom = ::RegisterClassW( &m_window_class );
                RTL_WINAPI_CHECK( atom != 0 );

                BOOL result;
                RECT window_rect;

                if constexpr ( is_fullscreen )
                {
                    result = ::GetWindowRect( ::GetDesktopWindow(), &window_rect );
                    RTL_WINAPI_CHECK( result );
                }

                m_window_handle = ::CreateWindowExW(
                    WS_EX_APPWINDOW,
                    m_window_class.lpszClassName,
                    window_name,
                    is_fullscreen && !is_resizable ? fullscreen_style
                    : is_resizable                 ? resizable_style
                                                   : fixed_style,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    is_fullscreen && !is_resizable ? window_rect.right - window_rect.left
                                                   : CW_USEDEFAULT,
                    is_fullscreen && !is_resizable ? window_rect.bottom - window_rect.top
                                                   : CW_USEDEFAULT,
                    nullptr,
                    nullptr,
                    m_window_class.hInstance,
                    nullptr ); // TODO: this?
                RTL_WINAPI_CHECK( m_window_handle != nullptr );

    #if RTL_ENABLE_APP_RESIZE
                m_placement.length = sizeof( m_placement );

                if constexpr ( is_fullscreen )
                    set_fullscreen_mode( true );
    #endif

                static_assert( sizeof( this ) == sizeof( LONG_PTR ) );
                ::SetWindowLongPtrW(
                    m_window_handle, GWL_USERDATA, reinterpret_cast<LONG_PTR>( this ) );

                // TODO: Check order of subsequent calls
                ::ShowWindow( m_window_handle, SW_SHOW );

                result = ::UpdateWindow( m_window_handle );
                RTL_WINAPI_CHECK( result );

                HDC hdc = ::GetDC( m_window_handle );
                RTL_WINAPI_CHECK( hdc != nullptr );

                m_device_context_handle = ::CreateCompatibleDC( hdc );
                RTL_WINAPI_CHECK( m_device_context_handle != nullptr );

                ::ReleaseDC( m_window_handle, hdc );

                resize();
            }

            void window::resize()
            {
                [[maybe_unused]] BOOL result = ::GetClientRect( m_window_handle, &m_client_rect );
                RTL_WINAPI_CHECK( result );

                [[maybe_unused]] const int width = this->width();
                [[maybe_unused]] const int height = this->height();

    #if RTL_ENABLE_APP_SCREEN
                {
                    m_bitmap_info.bmiHeader.biWidth = width;
                    m_bitmap_info.bmiHeader.biHeight = -height;
                    m_bitmap_info.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
                    m_bitmap_info.bmiHeader.biPlanes = 1;
                    m_bitmap_info.bmiHeader.biBitCount = 24;
                    m_bitmap_info.bmiHeader.biCompression = BI_RGB;
                    m_bitmap_info.bmiHeader.biXPelsPerMeter = 0x130B;
                    m_bitmap_info.bmiHeader.biYPelsPerMeter = 0x130B;

                    RTL_ASSERT( m_bitmap_handle == nullptr );

                    m_bitmap_handle
                        = ::CreateDIBSection( m_device_context_handle,
                                              &m_bitmap_info,
                                              DIB_RGB_COLORS,
                                              reinterpret_cast<void**>( &m_output.screen.pixels ),
                                              nullptr,
                                              0 );
                    RTL_WINAPI_CHECK( m_bitmap_handle != nullptr );

                    constexpr size_t sizeof_rgb = 3;
                    constexpr size_t align = sizeof( LONG );

                    m_output.screen.pitch
                        = ( ( ( sizeof_rgb * width ) + align - 1 ) / align ) * align;
                }

                m_output.screen.width = width;
                m_output.screen.height = height;
    #endif

    #if RTL_ENABLE_APP_OSD
                init_osd_text( width, height );
    #endif
            }

            void window::destroy_resizable_components()
            {
    #if RTL_ENABLE_APP_OSD
                if ( m_osd_font )
                {
                    [[maybe_unused]] BOOL result = ::DeleteObject( m_osd_font );
                    RTL_WINAPI_CHECK( result );
                    m_osd_font = nullptr;
                }
    #endif

    #if RTL_ENABLE_APP_SCREEN
                if ( m_bitmap_handle )
                {
                    [[maybe_unused]] BOOL result = ::DeleteObject( m_bitmap_handle );
                    RTL_WINAPI_CHECK( result );
                    m_bitmap_handle = nullptr;
                }
    #endif
            }

            void window::destroy()
            {
                destroy_resizable_components();

                BOOL result;
                result = ::DeleteDC( m_device_context_handle );
                RTL_WINAPI_CHECK( result );

                result = ::DestroyWindow( m_window_handle );
                RTL_WINAPI_CHECK( result );
            }

    #if RTL_ENABLE_APP_RESIZE
            void window::set_fullscreen_mode( bool fullscreen )
            {
                m_fullscreen = fullscreen;

                [[maybe_unused]] BOOL result;

                if ( m_fullscreen )
                {
                    result = ::GetWindowPlacement( m_window_handle, &m_placement );
                    RTL_WINAPI_CHECK( result );

                    [[maybe_unused]] LONG res = ::SetWindowLongW(
                        m_window_handle,
                        GWL_STYLE,
                        (LONG)( m_fullscreen ? fullscreen_style : resizable_style ) );
                    RTL_WINAPI_CHECK( res != 0 );

                    RECT rect;
                    result = ::GetWindowRect( ::GetDesktopWindow(), &rect );
                    RTL_WINAPI_CHECK( result );

                    result = ::SetWindowPos( m_window_handle,
                                             HWND_TOP,
                                             0,
                                             0,
                                             rect.right - rect.left,
                                             rect.bottom - rect.top,
                                             SWP_SHOWWINDOW );
                    RTL_WINAPI_CHECK( result );
                }
                else
                {
                    [[maybe_unused]] LONG res = ::SetWindowLongW(
                        m_window_handle,
                        GWL_STYLE,
                        (LONG)( m_fullscreen ? fullscreen_style : resizable_style ) );
                    RTL_WINAPI_CHECK( res != 0 );

                    result = ::SetWindowPlacement( m_window_handle, &m_placement );
                    RTL_WINAPI_CHECK( result );
                }
            }
    #endif

            LRESULT window::wnd_proc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
            {
                window* that
                    = reinterpret_cast<window*>( ::GetWindowLongPtrW( hWnd, GWL_USERDATA ) );

                switch ( uMsg )
                {
                case WM_CLOSE:
                    ::PostQuitMessage( 0 );
                    return 0;

    #if RTL_ENABLE_APP_KEYS
                case WM_KEYDOWN:
                {
                    const int key = static_cast<int>( virtual_key_to_enum( wParam ) );

                    if ( !that->m_input.keys.state[key] )
                        that->m_input.keys.pressed[key] = true;

                    that->m_input.keys.state[key] = true;
                    return 0;
                }

                case WM_KEYUP:
                {
                    const int key = static_cast<int>( virtual_key_to_enum( wParam ) );
                    that->m_input.keys.state[key] = false;
                    return 0;
                }
    #endif

    #if RTL_ENABLE_APP_RESIZE
                case WM_SIZING:
                {
                    if ( that )
                        that->m_sizing = true;

                    return 0;
                }

                case WM_EXITSIZEMOVE:
                {
                    if ( that )
                        that->m_sizing = false;

                    break;
                }

                case WM_SIZE:
                {
                    if ( wParam != SIZE_MINIMIZED )
                    {
                        if ( that )
                            that->m_sized = true;
                    }

                    break;
                }

                case WM_GETMINMAXINFO:
                {
                    if ( that )
                    {
                        MINMAXINFO* minmax = reinterpret_cast<MINMAXINFO*>( lParam );
                        minmax->ptMinTrackSize.x = minimal_width;
                        minmax->ptMinTrackSize.y = minimal_height;
                    }

                    break;
                }

    #endif

                case WM_PAINT:
                {
    #if RTL_ENABLE_APP_RESIZE
                    if ( that->m_sizing )
                        break;
    #endif
                    PAINTSTRUCT ps;

                    HDC hdc = ::BeginPaint( hWnd, &ps );
                    RTL_WINAPI_CHECK( hdc != nullptr );

                    if ( that->m_device_context_handle )
                    {
    #if RTL_ENABLE_APP_OSD
                        that->draw_osd_text();
    #endif

    #if RTL_ENABLE_APP_SCREEN
                        [[maybe_unused]] HGDIOBJ object = ::SelectObject(
                            that->m_device_context_handle, that->m_bitmap_handle );
                        RTL_WINAPI_CHECK( object != nullptr );
                        RTL_ASSERT( ::GetObjectType( object ) == OBJ_BITMAP );

                        const int width = that->m_bitmap_info.bmiHeader.biWidth;
                        const int height = -that->m_bitmap_info.bmiHeader.biHeight;

                        RTL_ASSERT( width <= that->width() );
                        RTL_ASSERT( height <= that->height() );

                        [[maybe_unused]] BOOL result = ::BitBlt( hdc,
                                                                 ( that->width() - width ) / 2,
                                                                 ( that->height() - height ) / 2,
                                                                 width,
                                                                 height,
                                                                 that->m_device_context_handle,
                                                                 0,
                                                                 0,
                                                                 SRCCOPY );
                        RTL_WINAPI_CHECK( result );

                            // TODO: This call brokes font rendering, deal with it later
                            // object = ::SelectObject( that->m_device_context_handle, object );
                            // RTL_ASSERT( object == that->m_bitmap_handle );
    #endif
                    }

                    [[maybe_unused]] BOOL result = ::EndPaint( hWnd, &ps );
                    RTL_WINAPI_CHECK( result );
                    return 0;
                }

                case WM_SETCURSOR:
                    if constexpr ( !has_cursor || is_resizable )
                    {
                        if ( LOWORD( lParam ) == HTCLIENT )
                        {
                            ::SetCursor( !has_cursor || that->fullscreen()
                                             ? nullptr
                                             : that->m_window_class.hCursor );
                            return TRUE;
                        }
                    }

                    break;
                }

                return ::DefWindowProcW( hWnd, uMsg, wParam, lParam );
            }

            void window::invalidate()
            {
                [[maybe_unused]] BOOL result = ::InvalidateRect( m_window_handle, nullptr, FALSE );
                RTL_WINAPI_CHECK( result );
            }

            void window::update( [[maybe_unused]] application::reset_function* on_resize,
                                 application::update_function*                 on_update )
            {
                [[maybe_unused]] BOOL result = ::GdiFlush();
                RTL_WINAPI_CHECK( result );

    #if RTL_ENABLE_APP_CLOCK
                m_input.clock.thirds = static_cast<int32_t>( ::GetTickCount() )
                                       * application::input::clock::measure / 1000;
    #endif

    #if RTL_ENABLE_APP_RESIZE
                if ( m_sized )
                {
                    destroy_resizable_components();
                    resize();
                    on_resize();
                    m_sized = false;
                }
    #endif

                auto action = on_update( m_input, m_output );

    #if RTL_ENABLE_APP_KEYS
                for ( size_t i = 0; i < static_cast<size_t>( keyboard::keys::count ); ++i )
                    m_input.keys.pressed[i] = false;
    #endif

                switch ( action )
                {
                case application::action::close:
                    ::PostQuitMessage( 0 );
                    break;

    #if RTL_ENABLE_APP_RESIZE
                case application::action::toggle_fullscreen:
                    set_fullscreen_mode( !m_fullscreen );
                    break;
    #endif

                case application::action::none:
                default:
                    invalidate();
                    break;
                }
            }

            window g_window;

        } // namespace win

    } // namespace impl

    void application::run( const wchar_t*   app_name,
                           reset_function*  on_reset,
                           update_function* on_update )
    {
        impl::win::g_window.create( app_name );

        on_reset();

        MSG msg{ 0 };

    #if 0
        for ( BOOL ret; ( ret = ::GetMessageW( &msg, nullptr, 0, 0 ) ) != 0; )
        {
            ::TranslateMessage( &msg );
            ::DispatchMessageW( &msg ); // TODO: result => exit or load next
        }
    #else
        for ( ; msg.message != WM_QUIT; )
        {
            while ( ::PeekMessageW( &msg, nullptr, 0, 0, PM_REMOVE ) )
            {
                if ( msg.message == WM_QUIT )
                    break;

                ::TranslateMessage( &msg );
                ::DispatchMessageW( &msg );
            }

            // TODO: wait for signal to update
            // TODO: run processing in separate thread
            // TODO: return time for next start or -1 for infinite wait
            impl::win::g_window.update( on_reset, on_update );
        }
    #endif

        impl::win::g_window.destroy();
    }

    application& application::instance()
    {
        static application g_app;
        return g_app;
    }
} // namespace rtl

#endif
