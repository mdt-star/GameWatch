#include "windowmonitor.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>

static QString cfStringToQString(CFStringRef str)
{
    if (!str) return QString();
    CFIndex length = CFStringGetLength(str);
    if (length == 0) return QString();
    
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    char *buffer = new char[maxSize + 1];
    
    if (CFStringGetCString(str, buffer, maxSize + 1, kCFStringEncodingUTF8)) {
        QString result = QString::fromUtf8(buffer);
        delete[] buffer;
        return result;
    }
    
    delete[] buffer;
    return QString();
}

QStringList WindowMonitor::allWindowTitles()
{
    QStringList titles;

    CFArrayRef windowList = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);

    if (!windowList) {
        return titles;
    }

    CFIndex count = CFArrayGetCount(windowList);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex(windowList, i);
        
        CFStringRef title = (CFStringRef)CFDictionaryGetValue(dict, kCGWindowName);
        if (title) {
            QString qtTitle = cfStringToQString(title);
            if (!qtTitle.isEmpty()) {
                titles.append(qtTitle);
            }
        }
    }

    CFRelease(windowList);

    // Remove duplicates
    titles.removeDuplicates();
    return titles;
}