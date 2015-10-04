#include "Theme.hpp"
#include "../Misc/Exceptions.hpp"
#include <json/json.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iterator>
using namespace std;

namespace OSHGui
{
	namespace Drawing
	{
		void Theme::SetControlColorTheme(const Misc::AnsiString &controlClass, const Theme::ControlTheme &controlTheme)
		{
			controlThemes[controlClass] = controlTheme;
		}
		//---------------------------------------------------------------------------
		const Theme::ControlTheme& Theme::GetControlColorTheme(const Misc::AnsiString &controlClass) const
		{
			auto it = controlThemes.find(controlClass);
			if (it == controlThemes.end())
			{
				return DefaultColor;
			}

			return it->second;
		}
		//---------------------------------------------------------------------------
		void Theme::Load(const Misc::UnicodeString &pathToThemeFile)
		{
			Name = string();
			Author = string();
			controlThemes.clear();
			DefaultColor.ForeColor = Color();
			DefaultColor.BackColor = Color();
			//TODO: int ausgabe und einlesen
			ifstream file(pathToThemeFile);
			if (!file.bad())
			{
				stringstream ss;
				ss << file.rdbuf();

				Json::Value root;
				Json::Reader reader;
				if (reader.parse(ss.str(), root) == false)
				{
					throw Misc::InvalidThemeException("Can't parse theme.");
				}

				Name = root.get("name", "").asString();
				Author = root.get("author", "").asString();

				auto JsonToColor = [](const Json::Value &value) -> Color
				{
					if (value.size() == 4)
					{
						if (value.isArray())
						{
							if (value[0].isInt() && value[1].isInt()
							 && value[2].isInt() && value[3].isInt())
							{
								return Color::FromARGB(
									value[0].asInt(),
									value[1].asInt(),
									value[2].asInt(),
									value[3].asInt()
								);
							}
						}
						else if (value.isObject() && value.size() == 4)
						{
							if (value["a"].isInt() && value["r"].isInt()
							 && value["g"].isInt() && value["b"].isInt())
							{
								return Color::FromARGB(
									value["a"].asInt(),
									value["r"].asInt(),
									value["g"].asInt(),
									value["b"].asInt()
								);
							}
						}
					}
					else if (value.isString())
					{
						stringstream ss(value.asString());
						union
						{
							struct
							{
								uint8_t B;
								uint8_t G;
								uint8_t R;
								uint8_t A;
							};
							uint32_t ARGB;
						};
						ss >> hex >> ARGB;
						if (ss.good())
						{
							return Color::FromARGB(A, R, G, B);
						}
					}
					throw Misc::InvalidThemeException("Invalid color.");
				};

				if (!root["default"].isObject())
				{
					throw Misc::InvalidThemeException("'default' is missing.");
				}
				auto &defaultColor = root["default"];
				DefaultColor.ForeColor = JsonToColor(defaultColor["forecolor"]);
				DefaultColor.BackColor = JsonToColor(defaultColor["backcolor"]);

				if (root["themes"].isObject())
				{
					auto &themes = root["themes"];
					auto member = themes.getMemberNames();
					for (auto it = member.begin(); it != member.end(); ++it)
					{
						ControlTheme theme;
						theme.ForeColor = JsonToColor(themes[*it]["forecolor"]);
						theme.BackColor = JsonToColor(themes[*it]["backcolor"]);
						SetControlColorTheme(*it, theme);
					}
				}
			}
			else
			{
				throw Misc::InvalidThemeException("Can't open file.");
			}
		}
		//---------------------------------------------------------------------------
		bool Theme::Save(const Misc::UnicodeString &pathToThemeFile, ColorStyle style) const
		{
			ofstream file(pathToThemeFile);
			if (!file.bad())
			{
				Json::Value root;
				root["name"] = Name;
				root["author"] = Author;

				auto ColorToJson = [](Json::Value &value, const Color &color, ColorStyle style)
				{
					union
					{
						struct
						{
							uint8_t B;
							uint8_t G;
							uint8_t R;
							uint8_t A;
						};
						uint32_t ARGB;
					};
					ARGB = color.GetARGB();
					switch (style)
					{
						case Theme::Text:
							value["a"] = A;
							value["r"] = R;
							value["g"] = G;
							value["b"] = B;
							break;
						case Theme::Array:
							value.append(A);
							value.append(R);
							value.append(G);
							value.append(B);
							break;
						case Theme::Integer:
							stringstream ss;
							ss << setfill('0') << setw(8) << std::hex << ARGB;
							value = ss.str();
							break;
					}
				};

				auto &defaultColor = root["default"];
				ColorToJson(defaultColor["forecolor"], DefaultColor.ForeColor, style);
				ColorToJson(defaultColor["backcolor"], DefaultColor.BackColor, style);

				auto &themes = root["themes"];
				for (auto it = controlThemes.begin(); it != controlThemes.end(); ++it)
				{
					Json::Value controlClass;

					ColorToJson(controlClass["forecolor"], it->second.ForeColor, style);
					ColorToJson(controlClass["backcolor"], it->second.BackColor, style);

					themes[it->first] = controlClass;
				}

				Json::StyledWriter writer;
				file << writer.write(root);

				return file.good();
			}

			return false;
		}
		//---------------------------------------------------------------------------
	}
}