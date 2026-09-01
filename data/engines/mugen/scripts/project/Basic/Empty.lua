-- Project template: a one-page wizard that makes a blank project of the chosen type.
-- Templates run from top to bottom and return the new project, or nil to cancel.

local wizard = FF.wizard()
wizard.windowTitle = FF.translate("Empty project")

local page = FF.wizardPage()
page.title = FF.translate("Empty project")
page.subTitle = FF.translate("Choose the type of the new blank project")

local choices = {
    { FF.translate("Character"), MUGEN.ProjectType.Character },
    { FF.translate("Stage"), MUGEN.ProjectType.Stage },
    { FF.translate("Font"), MUGEN.ProjectType.Font },
    { FF.translate("Storyboard"), MUGEN.ProjectType.Storyboard },
}

local layout = FF.verticalLayout(page)
local buttons = {}
for i, choice in ipairs(choices) do
    local button = FF.radioButton(choice[1], page)
    button.checked = i == 1
    layout:addWidget(button)
    buttons[i] = button
end
wizard:addPage(page)

if wizard:exec() == 0 then
    return nil
end

for i, button in ipairs(buttons) do
    if button.checked then
        return FF.emptyProject("mugen", choices[i][2])
    end
end
return nil
