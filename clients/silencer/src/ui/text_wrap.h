#ifndef SILENCER_UI_TEXT_WRAP_H
#define SILENCER_UI_TEXT_WRAP_H

namespace silencer {
namespace ui {

char * WordWrapText(const char * text, unsigned int maxlength, const char * breakchar = "\n");

}
}

#endif
