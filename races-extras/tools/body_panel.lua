-- CoA: in-game Body panel (body sliders of the local character), opened by the "Body" button next to the Collections tabs (beside Vanity).
-- Sliders run -1..1 and edit the character live; the values are kept per character (WTF\EsteriaBody.txt). Save writes them right away.
local CoABody = { rows = {}, group = "body" }

local function CoABody_Pretty(name)
	return (name:gsub("(%l)(%u)", "%1 %2"))
end

local function CoABody_Refresh()
	local frame = CoABodyFrame
	if not frame then return end
	local shown = 0
	local y = 0
	for _, row in ipairs(CoABody.rows) do
		local show = row.group == CoABody.group and GetBodyShapeSupport(row.name) ~= false
		if show then
			row:Show()
			row:ClearAllPoints()
			row:SetPoint("TOPLEFT", frame.content, "TOPLEFT", 0, -y)
			y = y + 34
			shown = shown + 1
			local lo, hi = GetBodyShapeRange(row.name)
			row.slider:SetMinMaxValues(lo or -1, hi or 1)
			row.updating = true
			row.slider:SetValue(GetBodyShape(row.name) or 0)
			row.updating = false
		else
			row:Hide()
		end
	end
	frame.content:SetHeight(math.max(y, 10))
	frame.empty:SetShown(shown == 0)
end

local function CoABody_Build()
	if CoABodyFrame then return CoABodyFrame end
	local frame = CreateFrame("Frame", "CoABodyFrame", UIParent)
	frame:SetSize(300, 470)
	frame:SetFrameStrata("DIALOG")
	frame:SetBackdrop({
		bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
		edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
		tile = true, tileSize = 32, edgeSize = 32,
		insets = { left = 11, right = 12, top = 12, bottom = 11 },
	})
	frame:EnableMouse(true)
	frame:SetMovable(true)
	frame:RegisterForDrag("LeftButton")
	frame:SetScript("OnDragStart", frame.StartMoving)
	frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
	frame:SetPoint("LEFT", Collections or UIParent, "RIGHT", 6, 0)
	frame:Hide()

	local title = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
	title:SetPoint("TOP", 0, -16)
	title:SetText("Body Sliders")

	local function Tab(text, group, x)
		local b = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
		b:SetSize(90, 22)
		b:SetPoint("TOPLEFT", x, -42)
		b:SetText(text)
		b:SetScript("OnClick", function() CoABody.group = group; CoABody_Refresh() end)
		return b
	end
	Tab("Body", "body", 24)
	Tab("Face", "face", 120)

	local scroll = CreateFrame("ScrollFrame", "CoABodyScroll", frame, "UIPanelScrollFrameTemplate")
	scroll:SetPoint("TOPLEFT", 22, -72)
	scroll:SetPoint("BOTTOMRIGHT", -40, 52)
	local content = CreateFrame("Frame", nil, scroll)
	content:SetSize(220, 10)
	scroll:SetScrollChild(content)
	frame.content = content

	frame.empty = frame:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
	frame.empty:SetPoint("TOP", scroll, "TOP", 0, -20)
	frame.empty:SetWidth(210)
	frame.empty:SetText("This model has no sliders in this group.")
	frame.empty:Hide()

	local index = 0
	for _, group in ipairs({ "body", "face" }) do
		for _, name in ipairs({ GetBodyShapeNames(group) }) do
			index = index + 1
			local row = CreateFrame("Frame", nil, content)
			row:SetSize(220, 32)
			row.name, row.group = name, group
			local label = row:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
			label:SetPoint("TOPLEFT", 0, 0)
			label:SetText(CoABody_Pretty(name))
			local slider = CreateFrame("Slider", "CoABodySlider" .. index, row, "OptionsSliderTemplate")
			slider:SetPoint("TOPLEFT", 0, -14)
			slider:SetWidth(200)
			slider:SetHeight(14)
			slider:SetMinMaxValues(-1, 1)
			slider:SetValueStep(0.05)
			_G[slider:GetName() .. "Low"]:SetText("")
			_G[slider:GetName() .. "High"]:SetText("")
			_G[slider:GetName() .. "Text"]:SetText("")
			slider:SetScript("OnValueChanged", function(self, value)
				if row.updating then return end
				SetBodyShape(name, value)
			end)
			row.slider = slider
			row:Hide()
			table.insert(CoABody.rows, row)
		end
	end

	local function Button(text, width, x, onClick)
		local b = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
		b:SetSize(width, 24)
		b:SetPoint("BOTTOMLEFT", x, 18)
		b:SetText(text)
		b:SetScript("OnClick", onClick)
		return b
	end
	Button("Randomize", 80, 20, function() BodyShapeRandomize(); CoABody_Refresh() end)
	Button("Reset", 70, 104, function() ResetBodyShape(); CoABody_Refresh() end)
	local saved = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
	saved:SetPoint("BOTTOM", 0, 46)
	saved:SetText("")
	Button("Save", 70, 178, function()
		if SaveBodyShape then SaveBodyShape() end
		saved:SetText("|cff00ff00Saved for this character|r")
	end)
	frame:SetScript("OnShow", function() saved:SetText(""); CoABody_Refresh() end)

	local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
	close:SetPoint("TOPRIGHT", -4, -4)
	return frame
end

function CoABody_Toggle()
	local frame = CoABody_Build()
	if frame:IsShown() then frame:Hide() else frame:Show() end
end

-- the "Body" button, at the right end of the Collections window bottom edge (next to the tab row with Vanity)
function CoABody_AddButton(collections, anchorTab)
	if collections.CoABodyButton then return end
	local b = CreateFrame("Button", "CoABodyTabButton", collections, "UIPanelButtonTemplate")
	b:SetSize(80, 24)
	b:SetPoint("TOPRIGHT", collections, "BOTTOMRIGHT", -12, 8)
	b:SetText("Body")
	b:SetScript("OnClick", function() CoABody_Toggle() end)
	b:SetScript("OnEnter", function(self)
		GameTooltip:SetOwner(self, "ANCHOR_TOP")
		GameTooltip:SetText("Body Sliders")
		GameTooltip:AddLine("Change the shape of your character's body and face. Use Save to keep it.", 1, 1, 1, true)
		GameTooltip:Show()
	end)
	b:SetScript("OnLeave", function() GameTooltip:Hide() end)
	collections.CoABodyButton = b
end
