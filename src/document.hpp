/* Reverse Engineer's Hex Editor
 * Copyright (C) 2017-2020 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef REHEX_DOCUMENT_HPP
#define REHEX_DOCUMENT_HPP

#include <functional>
#include <jansson.h>
#include <list>
#include <memory>
#include <stdint.h>
#include <utility>
#include <wx/dataobj.h>
#include <wx/wx.h>

#include "buffer.hpp"
#include "ByteRangeMap.hpp"
#include "ByteRangeSet.hpp"
#include "NestedOffsetLengthMap.hpp"
#include "util.hpp"

namespace REHex {
	wxDECLARE_EVENT(EV_INSERT_TOGGLED,      wxCommandEvent);
	wxDECLARE_EVENT(EV_SELECTION_CHANGED,   wxCommandEvent);
	wxDECLARE_EVENT(EV_COMMENT_MODIFIED,    wxCommandEvent);
	wxDECLARE_EVENT(EV_UNDO_UPDATE,         wxCommandEvent);
	wxDECLARE_EVENT(EV_BECAME_CLEAN,        wxCommandEvent);
	wxDECLARE_EVENT(EV_BECAME_DIRTY,        wxCommandEvent);
	wxDECLARE_EVENT(EV_DISP_SETTING_CHANGED,wxCommandEvent);
	wxDECLARE_EVENT(EV_HIGHLIGHTS_CHANGED,  wxCommandEvent);
	wxDECLARE_EVENT(EV_TYPES_CHANGED,       wxCommandEvent);
	
	/**
	 * @brief Data and metadata of an open file.
	 *
	 * This class holds a Buffer of data in the file, metadata (comments, highlights, etc) and
	 * manages access and operations on them.
	*/
	class Document: public wxEvtHandler {
		public:
			/**
			 * @brief A comment in a Document.
			*/
			struct Comment
			{
				/**
				 * @brief The comment text.
				 *
				 * We use a shared_ptr here so that unmodified comment text isn't
				 * duplicated throughout undo_stack and redo_stack. This might be
				 * made obsolete in the future if we apply a similar technique to
				 * the comments/highlights copies as a whole.
				 *
				 * wxString is used rather than std::string as it is unicode-aware
				 * and will keep everything in order in memory and on-screen.
				*/
				std::shared_ptr<const wxString> text;
				
				/**
				 * @brief Create a new comment.
				 *
				 * @param text Comment text.
				*/
				Comment(const wxString &text);
				
				bool operator==(const Comment &rhs) const
				{
					return *text == *(rhs.text);
				}
				
				/**
				 * @brief Get a short preview of the comment, suitable for use as a wxMenuItem label.
				*/
				wxString menu_preview() const;
			};
			
			enum CursorState {
				CSTATE_HEX,
				CSTATE_HEX_MID,
				CSTATE_ASCII,
				CSTATE_SPECIAL,
				
				/* Only valid as parameter to _set_cursor_position(), will go
				 * CSTATE_HEX if in CSTATE_HEX_MID, else will use current state.
				*/
				CSTATE_GOTO,
				
				/* Only valid as parameter to data manipulation methods to use
				 * current value of cursor_state.
				*/
				CSTATE_CURRENT,
			};
			
			/**
			 * @brief Create a Document for a new file.
			*/
			Document();
			
			/**
			 * @brief Create a Document for an existing file on disk.
			*/
			Document(const std::string &filename);
			
			~Document();
			
			/**
			 * @brief Save any changes to the file and its metadata.
			*/
			void save();
			
			/**
			 * @brief Save the file to a new path.
			*/
			void save(const std::string &filename);
			
			/**
			 * @brief Get the user-visible title of the document.
			 *
			 * This will usually be the name of the file, or a locale-appropriate
			 * label like "Untitled" for new files.
			*/
			std::string get_title();
			
			/**
			 * @brief Get the filename of the document, or an empty string if there is no backing file.
			*/
			std::string get_filename();
			
			/**
			 * @brief Check if the document has any pending changes to be saved.
			*/
			bool is_dirty();
			
			/**
			 * @brief Check if the given byte in the backing file has been modified since the last save.
			*/
			bool is_byte_dirty(off_t offset) const;
			
			off_t get_cursor_position() const;
			CursorState get_cursor_state() const;
			void set_cursor_position(off_t off, CursorState cursor_state = CSTATE_GOTO);
			
			/**
			 * @brief Get the comments in the file.
			*/
			const NestedOffsetLengthMap<Comment> &get_comments() const;
			
			/**
			 * @brief Set a comment in the file.
			 *
			 * @param offset   Offset of byte range.
			 * @param length   Length of byte range.
			 * @param comment  Comment to set.
			 *
			 * Returns true on success, false if off and/or length is beyond the
			 * current size of the file, or the range is straddling the end of another
			 * existing comment.
			 *
			 * Comments can have a length of zero, in which case they are displayed at
			 * the given offset, but do not encompass a range of bytes.
			*/
			bool set_comment(off_t offset, off_t length, const Comment &comment);
			
			/**
			 * @brief Erase a comment in the file.
			 *
			 * @param offset  Offset of comment to erase.
			 * @param length  Length of comment to erase.
			 *
			 * Returns true on success, false if the comment was not found.
			*/
			bool erase_comment(off_t offset, off_t length);
			
			/**
			 * @brief Get the highlighted byte ranges in the file.
			*/
			const NestedOffsetLengthMap<int> &get_highlights() const;
			
			/**
			 * @brief Set a highlight on a range of bytes in the file.
			 *
			 * @param off                   Offset of byte range.
			 * @param length                Length of byte range.
			 * @param highlight_colour_idx  Highlight colour index (0 .. Palette::NUM_HIGHLIGHT_COLOURS - 1).
			 *
			 * Returns true on success, false if off and/or length is beyond the
			 * current size of the file, or the range is straddling the end of another
			 * existing highlight.
			*/
			bool set_highlight(off_t off, off_t length, int highlight_colour_idx);
			
			/**
			 * @brief Remove a highlight from the file.
			 *
			 * @param off     Offset of byte range.
			 * @param length  Length of byte range.
			 *
			 * The off and length parameters must exactly match the highlight to be
			 * removed. This constraint will be removed in the future and it will be
			 * possible to remove a portion of a highlight.
			 *
			 * Returns true on success, false if the highlight wasn't found.
			*/
			bool erase_highlight(off_t off, off_t length);
			
			/**
			 * @brief Get the mapping of byte ranges to data types in the file.
			*/
			const ByteRangeMap<std::string> &get_data_types() const;
			
			/**
			 * @brief Set a data type mapping in the file.
			 *
			 * @param offset Offset of data.
			 * @param length Length of data, in bytes.
			 * @param type   Type of data.
			 *
			 * Sets the type of a range of bytes in the file, the type should be an
			 * empty string for untyped data, or a type known to the DataTypeRegistry.
			 *
			 * Returns true on success, false if the offset and/or length is beyond the
			 * current size of the file.
			*/
			bool set_data_type(off_t offset, off_t length, const std::string &type);
			
			void handle_paste(wxWindow *modal_dialog_parent, const NestedOffsetLengthMap<Document::Comment> &clipboard_comments);
			
			/**
			 * @brief Undo the last change to the document.
			*/
			void undo();
			
			/**
			 * @brief Get a description of the last change to the document.
			*/
			const char *undo_desc();
			
			/**
			 * @brief Replay a change undone with the undo() method.
			*/
			void redo();
			
			/**
			 * @brief Get a description of the next change to be replayed.
			*/
			const char *redo_desc();
			
		#ifndef UNIT_TEST
		private:
		#endif
			struct TrackedChange
			{
				const char *desc;
				
				std::function< void() > undo;
				std::function< void() > redo;
				
				off_t       old_cpos_off;
				CursorState old_cursor_state;
				NestedOffsetLengthMap<Comment> old_comments;
				NestedOffsetLengthMap<int> old_highlights;
				ByteRangeMap<std::string> old_types;
				
				bool old_dirty;
				ByteRangeSet old_dirty_bytes;
			};
			
			Buffer *buffer;
			std::string filename;
			
			bool dirty;
			ByteRangeSet dirty_bytes;
			
			void set_dirty(bool dirty);
			
			NestedOffsetLengthMap<Comment> comments;
			NestedOffsetLengthMap<int> highlights; /* TODO: Change this to a ByteRangeMap. */
			ByteRangeMap<std::string> types;
			
			std::string title;
			
			off_t cpos_off{0};
			bool insert_mode{false};
			
			enum CursorState cursor_state;
			
			static const int UNDO_MAX = 64;
			std::list<REHex::Document::TrackedChange> undo_stack;
			std::list<REHex::Document::TrackedChange> redo_stack;
			
			void _set_cursor_position(off_t position, enum CursorState cursor_state);
			
			void _UNTRACKED_overwrite_data(off_t offset, const unsigned char *data, off_t length);
			void _UNTRACKED_insert_data(off_t offset, const unsigned char *data, off_t length);
			void _UNTRACKED_erase_data(off_t offset, off_t length);
			
			void _tracked_overwrite_data(const char *change_desc, off_t offset, const unsigned char *data, off_t length, off_t new_cursor_pos, CursorState new_cursor_state);
			void _tracked_insert_data(const char *change_desc, off_t offset, const unsigned char *data, off_t length, off_t new_cursor_pos, CursorState new_cursor_state);
			void _tracked_erase_data(const char *change_desc, off_t offset, off_t length, off_t new_cursor_pos, CursorState new_cursor_state);
			void _tracked_replace_data(const char *change_desc, off_t offset, off_t old_data_length, const unsigned char *new_data, off_t new_data_length, off_t new_cursor_pos, CursorState new_cursor_state);
			void _tracked_change(const char *desc, std::function< void() > do_func, std::function< void() > undo_func);
			
			json_t *_dump_metadata(bool& has_data);
			void _save_metadata(const std::string &filename);
			
			static NestedOffsetLengthMap<Comment> _load_comments(const json_t *meta, off_t buffer_length);
			static NestedOffsetLengthMap<int> _load_highlights(const json_t *meta, off_t buffer_length);
			static ByteRangeMap<std::string> _load_types(const json_t *meta, off_t buffer_length);
			void _load_metadata(const std::string &filename);
			
			void _raise_comment_modified();
			void _raise_undo_update();
			void _raise_dirty();
			void _raise_clean();
			void _raise_highlights_changed();
			void _raise_types_changed();
			
		public:
			/**
			 * @brief Read some data from the file.
			 * @see Buffer::read_data()
			*/
			std::vector<unsigned char> read_data(off_t offset, off_t max_length) const;
			
			/**
			 * @brief Return the current length of the file in bytes.
			*/
			off_t buffer_length();
			
			/**
			 * @brief Overwrite a range of bytes in the file.
			 *
			 * @param offset            File offset to write data at.
			 * @param data              Pointer to data buffer.
			 * @param length            Length of data to write.
			 * @param new_cursor_pos    New cursor position. Pass a negative value to not change cursor position.
			 * @param new_cursor_state  New cursor state. Pass CSTATE_CURRENT to not change the cursor state.
			 * @param change_desc       Description of change for undo history.
			*/
			void overwrite_data(off_t offset, const void *data, off_t length,                                            off_t new_cursor_pos = -1, CursorState new_cursor_state = CSTATE_CURRENT, const char *change_desc = "change data");
			
			/**
			 * @brief Insert a range of bytes into the file.
			 *
			 * @param offset            File offset to insert data at.
			 * @param data              Pointer to data buffer.
			 * @param length            Length of data to write.
			 * @param new_cursor_pos    New cursor position. Pass a negative value to not change cursor position.
			 * @param new_cursor_state  New cursor state. Pass CSTATE_CURRENT to not change the cursor state.
			 * @param change_desc       Description of change for undo history.
			*/
			void insert_data(off_t offset, const unsigned char *data, off_t length,                                      off_t new_cursor_pos = -1, CursorState new_cursor_state = CSTATE_CURRENT, const char *change_desc = "change data");
			
			/**
			 * @brief Erase a range of bytes in the file.
			 *
			 * @param offset            File offset to erase data from.
			 * @param length            Length of data to erase.
			 * @param new_cursor_pos    New cursor position. Pass a negative value to not change cursor position.
			 * @param new_cursor_state  New cursor state. Pass CSTATE_CURRENT to not change the cursor state.
			 * @param change_desc       Description of change for undo history.
			*/
			void erase_data(off_t offset, off_t length,                                                                  off_t new_cursor_pos = -1, CursorState new_cursor_state = CSTATE_CURRENT, const char *change_desc = "change data");
			
			/**
			 * @brief Replace a range of bytes in the file.
			 *
			 * @param offset            File offset to replace data at.
			 * @param old_data_length   Length of data to be replaced.
			 * @param new_data          Pointer to data buffer.
			 * @param new_data_length   Length of new data.
			 * @param new_cursor_pos    New cursor position. Pass a negative value to not change cursor position.
			 * @param new_cursor_state  New cursor state. Pass CSTATE_CURRENT to not change the cursor state.
			 * @param change_desc       Description of change for undo history.
			*/
			void replace_data(off_t offset, off_t old_data_length, const unsigned char *new_data, off_t new_data_length, off_t new_cursor_pos = -1, CursorState new_cursor_state = CSTATE_CURRENT, const char *change_desc = "change data");
	};
	
	/**
	 * @brief Data object that stores a list of comments.
	 *
	 * This class provides wxDataObject-compatible serialisation of one or more comments so
	 * that they can be copied via the clipboard.
	*/
	class CommentsDataObject: public wxCustomDataObject
	{
		private:
			struct Header
			{
				off_t file_offset;
				off_t file_length;
				
				size_t text_length;
			};
			
		public:
			/**
			 * @brief wxDataFormat used for comments in the clipboard.
			*/
			static const wxDataFormat format;
			
			/**
			 * @brief Construct an empty CommentsDataObject.
			*/
			CommentsDataObject();
			
			/**
			 * @brief Construct a CommentsDataObject from a list of comments.
			 *
			 * @param comments  List of iterators to comments to be serialised.
			 * @param base      Base offset to be subtracted from the offset of each comment.
			*/
			CommentsDataObject(const std::list<NestedOffsetLengthMap<REHex::Document::Comment>::const_iterator> &comments, off_t base = 0);
			
			/**
			 * @brief Deserialise the CommentsDataObject and return the stored comments.
			*/
			NestedOffsetLengthMap<Document::Comment> get_comments() const;
			
			/**
			 * @brief Replace the serialised list of stored comments.
			 *
			 * @param comments  List of iterators to comments to be serialised.
			 * @param base      Base offset to be subtracted from the offset of each comment.
			*/
			void set_comments(const std::list<NestedOffsetLengthMap<REHex::Document::Comment>::const_iterator> &comments, off_t base = 0);
	};
}

#endif /* !REHEX_DOCUMENT_HPP */
