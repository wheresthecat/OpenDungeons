/*!
 * \file   Console.cpp
 * \date:  04 July 2011
 * \author StefanP.MUC
 * \brief  Ingame console
 */

/* TODO: decide and adjust the layout and prompt (size, position, color)
 * TODO: do intense testing that everything works
 * TODO: switch from TextRenderer to Console
 */

#include "ASWrapper.h"
#include "Console.h"
#include "InputManager.h"
#include "LogManager.h"
#include "ODApplication.h"
#include "ODFrameListener.h"
#include "RenderManager.h"

template<> Console* Ogre::Singleton<Console>::ms_Singleton = 0;

Console::Console() :
        //these two define how much text goes into the console
        consoleLineLength(100),
        consoleLineCount(10),
        visible(false),
        updateOverlay(true),
        allowTrivial(false),
        allowNormal(false),
        allowCritical(true),
        startLine(0),
        curHistPos(0)
{
    ODApplication::getSingleton().getRoot()->addFrameListener(this);

    Ogre::OverlayManager& olMgr = Ogre::OverlayManager::getSingleton();

    // Create a panel
    panel = static_cast<Ogre::OverlayContainer*>(
            olMgr.createOverlayElement("Panel", "ConsolePanel"));
    panel->setPosition(0, 0.7);
    panel->setDimensions(1, 0.3);
    panel->setMaterialName("console/background");

    // Create a text area
    textbox = olMgr.createOverlayElement("TextArea", "ConsoleText");
    textbox->setPosition(0, 0);
    textbox->setParameter("font_name", "FreeMono");
    textbox->setParameter("char_height", "0.02");

    // Create an overlay, and add the panel
    overlay = olMgr.create("Console");
    overlay->add2D(panel);

    // Add the text area to the panel
    panel->addChild(textbox);

    // Ogre::LogManager::getSingleton().getDefaultLog()->addListener(this);
}

Console::~Console()
{
    delete panel;
    delete textbox;
    delete overlay;
}

/*! \brief handles the key input
 *
 */
void Console::onKeyPressed(const OIS::KeyEvent &arg)
{
    if (!visible)
    {
        return;
    }

    switch(arg.key)
    {
        case OIS::KC_GRAVE:
        case OIS::KC_F12:
            Console::getSingleton().setVisible(false);
            ODFrameListener::getSingleton().setTerminalActive(false);
            InputManager::getSingleton().getKeyboard()->setTextTranslation(OIS::Keyboard::Off);
            break;

        case OIS::KC_RETURN:
        {
            //only do this for non-empty input
            if(!prompt.empty())
            {
                print(prompt);
                history.push_back(prompt);
                ++curHistPos;

                //split the input into it's space-separated "words"
                std::vector<Ogre::String> params = split(prompt, ' ');

                //TODO: this won't be needed after we have a good command handling
                //then we only should need something like executeCommand(params);
                //where params[0] is the command and all other elements are arguments
                Ogre::String command = params[0];
                Ogre::String arguments = "";
                for(size_t i = 1; i< params.size(); ++i)
                {
                    arguments += params[i] + ' ';
                }

                // Force command to lower case
                //TODO: later do this only for params[0]
                std::transform(command.begin(), command.end(), command.begin(), ::tolower);

                ODFrameListener::getSingleton().executePromptCommand(command, arguments);

                //TODO: this works already, only the script is missing
                //      -> convert executePromptCommand() to AS
                //ASWrapper::getSingleton().executeConsoleCommand(params);

                prompt = "";
            }
            else
            {
                //set history position back to last entry
                curHistPos = history.size();
            }
            break;
        }

        case OIS::KC_BACK:
            prompt = prompt.substr(0, prompt.length() - 1);
            break;

        case OIS::KC_PGUP:
            if (startLine > 0)
            {
                --startLine;
            }
            break;

        case OIS::KC_PGDOWN:
            if (startLine < lines.size())
            {
                ++startLine;
            }
            break;

        case OIS::KC_UP:
            scrollHistory(true);
            prompt = history[curHistPos];
            break;

        case OIS::KC_DOWN:
            scrollHistory(false);
            prompt = history[curHistPos];
            break;

        case OIS::KC_F10:
        {
            LogManager::getSingleton().logMessage("RTSS test----------");
            RenderManager::getSingleton().rtssTest();
            break;
        }

        default:
            if (std::string("abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ,.<>/?1234567890-=\\!@#$%^&*()_+|;\':\"[]{}").find(
                    arg.text) != std::string::npos)
            {
                prompt += arg.text;
            }
            break;
    }

    updateOverlay = true;
}

/*! \brief Defines the action on starting the current frame
 *
 *  The Console listener checks if it needs updating and if it does it will
 *  redraw itself with the new text
 */
bool Console::frameStarted(const Ogre::FrameEvent &evt)
{
    if(updateOverlay)
    {
        Ogre::String text;
        std::list<Ogre::String>::iterator i, start, end;

        //make sure is in range
        if(startLine > lines.size())
        {
            startLine = lines.size();
        }

        start = lines.begin();
        for (unsigned int c = 0; c < startLine; ++c)
        {
            ++start;
        }

        end = start;
        for (unsigned int c = 0; c < consoleLineCount; ++c)
        {
            if (end == lines.end())
            {
                break;
            }
            ++end;
        }

        for (i = start; i != end; ++i)
        {
            text += (*i) + "\n";
        }

        //add the prompt
        text += "] " + prompt;

        textbox->setCaption(text);
        updateOverlay = false;
    }
    return true;
}

/*! \brief print text to the console
 *
 * This function automatically checks if there are linebreaks in the text
 * and separates the text into separate strings
 *
 * \param text The text to be added to the console
 */
void Console::print(const Ogre::String &text)
{
    std::vector<Ogre::String> newLines = split(text, '\n');
    lines.insert(lines.end(), newLines.begin(), newLines.end());

    startLine = (lines.size() > consoleLineCount)
                            ? lines.size() - consoleLineCount
                                    : 0;

    updateOverlay = true;
}

/*! \brief what happens after frame
 *
 */
bool Console::frameEnded(const Ogre::FrameEvent &evt)
{
    return true;
}

/*! \brief show or hide the console manually
 *
 */
void Console::setVisible(const bool& newState)
{
    visible = newState;
    Gui::getSingleton().setVisible(!visible);
    checkVisibility();
}

/*! \brief enables or disables the console, depending on what state it has
 *
 */
void Console::toggleVisibility()
{
    visible = !visible;
    Gui::getSingleton().setVisible(!visible);
    checkVisibility();
}

/*! \brief Does the actual showing/hiding depending on bool visible
 *
 */
void Console::checkVisibility()
{
    if(visible)
    {
        overlay->show();
    }
    else
    {
        overlay->hide();
    }
}

/*! \brief Splits a string on every occurance of splitChar
 *
 *  \return A vector of all splitted sub strings
 *
 *  \param str The str to be splitted
 *  \param splitChar The character that defines the split positions
 */
std::vector<Ogre::String> Console::split(const Ogre::String& str, const char& splitChar)
{
    std::vector<Ogre::String> splittedStrings;
    size_t lastPos = 0, pos = 0;
    do
    {
        pos = str.find(splitChar, lastPos);
        splittedStrings.push_back(str.substr(lastPos, pos - lastPos));
        lastPos = pos + 1; //next time start AFTER the last space
    }while(pos != std::string::npos);

    return splittedStrings;
}

/*! \brief Send logged messages also to the Console
 *
 * We only allow critical messages to the console. Non-critical messages would
 * pollute the console window and make it hardly readable.
 */
void Console::messageLogged(const Ogre::String & message, Ogre::LogMessageLevel lml, bool maskDebug, const Ogre::String & logName)
{
    //test if the logLevel is allowed, if not then return
    switch(lml)
    {
        case Ogre::LML_CRITICAL:
            if(!allowCritical){return;}
            break;

        case Ogre::LML_TRIVIAL:
            if(!allowTrivial){return;}
            break;

        case Ogre::LML_NORMAL:
            if(!allowNormal){return;}
            break;

        default:
            return;
    }

    //if it was allowed then print the message
    print(logName + ": " + message);
}

/*! \brief Scrolls through the history of user entered commands
 *
 *  \param direction true means going up (old), false means going down (new)
 */
void Console::scrollHistory(const bool& direction)
{
    if(direction)
    {
        //don't go unter 0, it's an unsigned int and the minimum index!
        if(curHistPos == 0)
        {
            return;
        }
        else
        {
            --curHistPos;
        }
    }
    else
    {
        //don't go over maximum index!
        if(++curHistPos > history.size() - 1)
        {
            curHistPos = history.size() - 1;
        }

    }
}
