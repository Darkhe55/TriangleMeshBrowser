// src/utils/I18n.h
// 多语言支持 - 按 key 取中文/英文文本
#pragma once

namespace prism::i18n {

enum class Language { Zh, En };

void setLanguage(Language lang) noexcept;
Language language() noexcept;

// 按 key 取当前语言的文本;无匹配时返回 key 本身
const char* tr(const char* key);

} // namespace prism::i18n
