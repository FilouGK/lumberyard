/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 */
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>
#include <LyzardSDK/Base.h>

namespace Capabilities
{
    class Capability
    {
    public:
        Capability() = default;

        Capability(const AZStd::string& identifier,
            const AZStd::string& description,
            const AZStd::string& tooltip,
            const AZStd::vector<AZStd::string>& tags,
            const AZStd::vector<AZStd::string>& hosts,
            const AZStd::vector<AZStd::string>& categories,
            const bool isDefault)
            : m_identifier(identifier)
            , m_description(description)
            , m_tooltip(tooltip)
            , m_tags(tags)
            , m_hosts(hosts)
            , m_categories(categories)
            , m_isDefault(isDefault)
            , m_isEnabled(false)
        {};

        ~Capability() = default;

        const AZStd::string& GetIdentifier() const { return m_identifier; }
        const AZStd::string& GetDescription() const { return m_description; }
        const AZStd::string& GetTooltip() const { return m_tooltip; }
        const AZStd::vector<AZStd::string>& GetTags() const { return m_tags; }
        const AZStd::vector<AZStd::string>& GetHosts() const { return m_hosts; }
        const AZStd::vector<AZStd::string>& GetCategories() const { return m_categories; }
        bool IsDefault() const { return m_isDefault; }
        bool IsEnabled() const { return m_isEnabled; }

        void EnableCapability(bool isEnabled) { m_isEnabled = isEnabled; }

    private:
        AZStd::string m_identifier;
        AZStd::string m_description;
        AZStd::string m_tooltip;
        AZStd::vector<AZStd::string> m_tags;
        AZStd::vector<AZStd::string> m_hosts;
        AZStd::vector<AZStd::string> m_categories;
        bool m_isDefault = false;
        bool m_isEnabled = false;
    };

    /**
     * Bus for requesting Capabilities operations.
     */
    class CapabilitiesRequests
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual ~CapabilitiesRequests() = default;

        /**
         * Loads capabilites from configuration files starting at engine root down to the specified working directory. Persists settings of the loaded capabilities.
         *
         * \param[in] workingDir            The application's working directory.
         * \param[in] configFilePath        The name of the configuration file.
         * \param[in] persistenceFilePath   The filename or relative path to persistence settings file.
         *
         * \returns                         An AZ::Outcome describing the success or failure state of the procedure.
         */
        virtual Lyzard::StringOutcome LoadCapabilities(const AZStd::string& workingDir, 
            const AZStd::string& configFilePath, 
            const AZStd::string& persistenceFilePath) = 0;

        /**
         * Get a list of all the capabilities loaded by LoadCapabilities. Capabilities are represented by the Capability class.
         *
         * \returns     AZStd::vector of Capability objects.
         */
        virtual AZStd::vector<Capability> GetAllCapabilities() const = 0;

        /**
         * Sets the enable state of the capability with the provided identifier to ENABLED and persists the settings.
         *
         * \param[in] capabilityName    The identifier of the capability that will be enabled.
         *
         * \returns                     An AZ::Outcome describing the success or failure state of the procedure.
         */
        virtual Lyzard::StringOutcome EnableCapability(const AZStd::string& capabilityName) = 0;

        /**
         * Sets the enable state of the capability with the provided identifier to DISABLED and persists the settings.
         *
         * \param[in] capabilityName    The identifier of the capability that will be disabled.
         *
         * \returns                     An AZ::Outcome describing the success or failure state of the procedure.
         */
        virtual Lyzard::StringOutcome DisableCapability(const AZStd::string& capabilityName) = 0;

        /**
         * Creates a new capability and stores it in the user-modifiable configuration file. Also persists the settings.
         *
         * \param[in] capabilityObject      A Capability object of the new, custom capability.
         * \param[in] configFilePath        The path to the configuration file where it should be stored.
         *
         * \returns             An AZ::Outcome describing the success or failure state of the procedure.
         */
        virtual Lyzard::StringOutcome CreateCapability(const Capability& capabilityObject, const AZStd::string& configFilePath) = 0;

        /**
         * Checks if the provided tag appears in at least one of the enabled capabilities' tags vector.
         *
         * \param[in] tag       The tag to check.
         *
         * \returns             True if the tag appears in at least one of the enabled capabilities, else False.
         */
        virtual bool IsTagSet(const AZStd::string& tag) const = 0;

        /**
        * Return a list of paths to all the config files from the working directory. It starts the search from the working
        * directory all the way up to the engine root.
        *
        * \param[in] workingDir      The working directory to start the search from.
        * \param[in] configFileName  The name of the config file to look for.
        *
        * \returns             AZStd::vector of all the path to the config files found.
        */
        virtual AZStd::vector<AZStd::string> GetPathsOfConfigFiles(const AZStd::string& workingDir, const AZStd::string& configFileName) const = 0;
    };

    using CapabilitiesRequestsBus = AZ::EBus<CapabilitiesRequests>;
}
