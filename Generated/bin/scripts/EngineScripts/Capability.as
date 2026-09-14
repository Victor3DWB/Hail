// Changes to the base-class must be updated on the C++ side to reflect what is going on in the script.

shared class Capability
{
    // Allow scripts to create instances
    Capability()
    {
        // Create the C++ side of the proxy
        @m_baseCapabilityClass = Capability_t();
    }

    // The copy constructor performs a deep copy
    Capability(const Capability& o)
    {
        // Create a new C++ instance and copy content
        @m_baseCapabilityClass = Capability_t();
        m_baseCapabilityClass = o.m_baseCapabilityClass;
    }

    // Do a deep a copy of the C++ object
    Capability& opAssign(const Capability& o)
    {
        // copy content of C++ instance
        m_baseCapabilityClass = o.m_baseCapabilityClass; 
        return this; 
    }

    void Setup() { }
    void LateSetup() { }
    void OnOwnerDestroyed() { }
    bool ShouldActivate() { return false; }
    bool ShouldDeactivate() { return false; }
    void TickActive(float deltaTime) { }
    void TickInactive(float deltaTime) { }
    void OnActivated() { }
    void OnDeactivated() { }
    bool GetIsActive() const { return m_bActive; }
    float GetTimeDilation() const {  return 1.0; }

    // The C++ side properties is exposed to the script through accessors
    bool m_bActive
    {
        get const { return m_baseCapabilityClass.m_bActive; }
        set { m_baseCapabilityClass.m_bActive = value; }
    }
    bool m_bDidLateSetup
    {
        get { return m_baseCapabilityClass.m_bDidLateSetup; }
        set { m_baseCapabilityClass.m_bDidLateSetup = value; }
    }
    bool m_bDidLateSetupHello
    {
        get { return m_baseCapabilityClass.m_bDidLateSetupHello; }
        set { m_baseCapabilityClass.m_bDidLateSetupHello = value; }
    }
    float m_activeDuration
    {
        get { return m_baseCapabilityClass.m_activeDuration; }
        set { m_baseCapabilityClass.m_activeDuration = value; }
    }
    float m_inactiveDuration
    {
        get { return m_baseCapabilityClass.m_inactiveDuration; }
        set { m_baseCapabilityClass.m_inactiveDuration = value; }
    }
    ECapabilityTickGroup m_group
    {
        get { return m_baseCapabilityClass.m_group; }
        set { m_baseCapabilityClass.m_group = value; }
    }
    ECapabilityTickSubGroup m_subGroup
    {
        get { return m_baseCapabilityClass.m_subGroup; }
        set { m_baseCapabilityClass.m_subGroup = value; }
    }
    private uint m_owningEntityId
    {
        get { return m_baseCapabilityClass.m_owningEntityId; }
        set {  }
    }

    uint GetOwningEntity() { return m_owningEntityId; }
    // Hold a reference to the C++ side of the proxy
    private Capability_t @m_baseCapabilityClass;
};


