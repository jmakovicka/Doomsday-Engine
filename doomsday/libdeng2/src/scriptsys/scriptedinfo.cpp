/** @file scriptedinfo.cpp  Info document tree with script context.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/ScriptedInfo"
#include "de/Script"
#include "de/Process"
#include "de/ArrayValue"
#include "de/RecordValue"

namespace de {

static String const BLOCK_GROUP = "group";
static String const KEY_INHERIT = "inherits";

DENG2_PIMPL(ScriptedInfo)
{
    typedef Info::Element::Value InfoValue;

    Info info;                     ///< Original full parsed contents.
    QScopedPointer<Script> script; ///< Current script being executed.
    Process process;               ///< Execution context.
    String sourcePath;

    Instance(Public *i) : Base(i)
    {}

    void clear()
    {
        info.clear();
        process.clear();
        script.reset();
    }

    /**
     * Iterates through the parsed Info contents and processes each element.
     * Script blocks are parsed as Doomsday Script and executed in the local
     * process. Key/value elements become variables (and values) in the
     * process's global namespace.
     */
    void processAll()
    {
        processBlock(info.root());

        LOG_DEBUG("Processed contents:\n") << process.globals().asText();
    }

    void processElement(Info::Element const *element)
    {
        if(element->isBlock())
        {
            processBlock(*static_cast<Info::BlockElement const *>(element));
        }
        else if(element->isKey())
        {
            processKey(*static_cast<Info::KeyElement const *>(element));
        }
        else if(element->isList())
        {
            processList(*static_cast<Info::ListElement const *>(element));
        }
    }

    void executeWithContext(Info::BlockElement const *context)
    {
        Record &ns = process.globals();

        // The global "__this__" will point to the block where the script
        // is running.
        bool needRemoveThis = false;
        if(context)
        {
            String varName = variableName(*context);
            if(!varName.isEmpty())
            {
                if(!ns.has(varName))
                {
                    // If it doesn't exist yet, make sure it does.
                    ns.addRecord(varName);
                }
                ns.add("__this__") = new RecordValue(&ns.subrecord(varName));
                needRemoveThis = true;
            }
        }

        // Execute the current script.
        process.execute();

        if(needRemoveThis)
        {
            delete &ns["__this__"];
        }
    }

    void inherit(Info::BlockElement const &block, InfoValue const &target)
    {
        if(block.name().isEmpty())
        {
            // Nameless blocks cannot be inherited into.
            return;
        }

        String varName = variableName(block);
        if(!varName.isEmpty())
        {
            Record &ns = process.globals();            
            String targetName = target;
            if(!ns.has(targetName))
            {
                // Assume it's an identifier rather than a regular variable.
                targetName = targetName.toLower();
            }

            ns.add(varName.concatenatePath("__inherit__", '.')) =
                    new TextValue(targetName);

            LOG_DEV_TRACE("setting __inherit__ of %s %s (%p) to %s",
                          block.blockType() << block.name() << &block << targetName);

            DENG2_ASSERT(!varName.isEmpty());
            DENG2_ASSERT(!targetName.isEmpty());

            // Copy all present members of the target record.
            ns.subrecord(varName)
                    .copyMembersFrom(ns[targetName].value<RecordValue>().dereference(),
                                     Record::IgnoreDoubleUnderscoreMembers);
        }
    }

    void inheritFromAncestors(Info::BlockElement const &block, Info::BlockElement const *from)
    {
        if(!from) return;

        // The highest ancestor goes first.
        if(from->parent())
        {
            inheritFromAncestors(block, from->parent());
        }

        // This only applies to groups.
        if(from->blockType() == BLOCK_GROUP)
        {
            if(Info::KeyElement *key = from->findAs<Info::KeyElement>(KEY_INHERIT))
            {
                inherit(block, key->value());
            }
        }
    }

    void processBlock(Info::BlockElement const &block)
    {
        Record &ns = process.globals();

        if(Info::Element *condition = block.find("condition"))
        {
            // Any block will be ignored if its condition is false.
            QScopedPointer<Value> result(evaluate(condition->values().first(), 0));
            if(result.isNull() || result->isFalse())
            {
                return;
            }
        }

        // Inherit from all nameless parent blocks.
        inheritFromAncestors(block, block.parent());

        // Direct inheritance.
        if(Info::KeyElement *key = block.findAs<Info::KeyElement>(KEY_INHERIT))
        {
            // Check for special attributes.
            if(key->flags().testFlag(Info::KeyElement::Attribute))
            {
                // Inherit contents of an existing Record.
                inherit(block, key->value());
            }
        }

        // Script blocks are executed now.
        if(block.blockType() == "script")
        {
            DENG2_ASSERT(block.find("script") != 0);
            DENG2_ASSERT(process.state() == Process::Stopped);

            script.reset(new Script(block.find("script")->values().first()));
            script->setPath(sourcePath); // where the source comes from
            process.run(*script);
            executeWithContext(block.parent());
        }
        else
        {
            // Block type placed into a special variable (only with named blocks, though).
            if(!block.name().isEmpty())
            {
                String varName = variableName(block).concatenatePath("__type__", '.');
                ns.add(varName) = new TextValue(block.blockType());
            }

            foreach(Info::Element const *sub, block.contentsInOrder())
            {
                // Handle special elements.
                if(sub->name() == "condition" || sub->name() == "inherits")
                {
                    // Already handled.
                    continue;
                }
                processElement(sub);
            }
        }
    }

    /**
     * Determines the name of the variable representing an element. All named
     * containing parent blocks contribute to the variable name.
     *
     * @param element  Element whose variable name to determine.
     *
     * @return Variable name in the form <tt>ancestor.parent.elementname</tt>.
     */
    String variableName(Info::Element const &element)
    {
        String varName = element.name();
        for(Info::BlockElement *b = element.parent(); b != 0; b = b->parent())
        {
            if(!b->name().isEmpty())
            {
                if(varName.isEmpty())
                    varName = b->name();
                else
                    varName = b->name().concatenatePath(varName, '.');
            }
        }
        return varName;
    }

    Value *evaluate(String const &source, Info::BlockElement const *context)
    {
        script.reset(new Script(source));
        process.run(*script);
        executeWithContext(context);
        return process.context().evaluator().result().duplicate();
    }

    /**
     * Constructs a Value from the value of an element. If the element value
     * has been marked with the semantic hint for scripting, it will be
     * evaluated as a script. The global __this__ will be pointed to the
     * Record representing the @a context block.
     *
     * @param rawValue  Value of an element.
     * @param context   Containing block element.
     *
     * @return Value instance. Caller gets ownership.
     */
    Value *makeValue(InfoValue const &rawValue, Info::BlockElement const *context)
    {
        if(rawValue.flags.testFlag(InfoValue::Script))
        {
            return evaluate(rawValue.text, context);
        }
        return new TextValue(rawValue.text);
    }

    void processKey(Info::KeyElement const &key)
    {
        QScopedPointer<Value> v(makeValue(key.value(), key.parent()));
        process.globals().add(variableName(key)) = v.take();
    }

    void processList(Info::ListElement const &list)
    {
        ArrayValue* av = new ArrayValue;
        foreach(InfoValue const &v, list.values())
        {
            *av << makeValue(v, list.parent());
        }
        process.globals().addArray(variableName(list), av);
    }
};

ScriptedInfo::ScriptedInfo() : d(new Instance(this))
{}

void ScriptedInfo::clear()
{
    d->clear();
}

void ScriptedInfo::parse(String const &source)
{
    d->clear();
    d->info.parse(source);
    d->processAll();
}

void ScriptedInfo::parse(File const &file)
{
    d->sourcePath = file.path();
    parse(String::fromUtf8(Block(file)));
}

Value *ScriptedInfo::evaluate(String const &source)
{
    return d->evaluate(source, 0);
}

Record &ScriptedInfo::names()
{
    return d->process.globals();
}

Record const &ScriptedInfo::names() const
{
    return d->process.globals();
}

} // namespace de
