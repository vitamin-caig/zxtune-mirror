l10n_languages := ru

l10n_dir = $(path_step)/l10n
mo_translation_data := $(l10n_dir)/gettext.zip

ifeq ($(host),windows)
fix_path_cmd = $(subst /,\,$(1))
else
fix_path_cmd = $(1)
endif

mo_translation_files = $(wildcard $(foreach lang,$(l10n_languages),$(l10n_dir)/$(lang)/*.mo))

$(mo_translation_data): $(mo_translation_files)
	(cd $(call fix_path_cmd,$(l10n_dir)) && zip -v9uD $(notdir $@) $(call fix_path_cmd,$(addprefix $(CURDIR)/,$^)))

%.mo: %.po
		msgfmt --output $@ $^

vpath %.po $(l10n_dir)
