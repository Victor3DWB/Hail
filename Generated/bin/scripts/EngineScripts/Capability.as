// Changes to the base-class must be updated on the C++ side to reflect what is going on in the script.

shared class Capability
{
    // Allow scripts to create instances
    Capability()
    {
        // Create the C++ side of the proxy
        @m_obj = Capability_t();
    }

    // The copy constructor performs a deep copy
    Capability(const Capability& o)
    {
        // Create a new C++ instance and copy content
        @m_obj = Capability_t();
        m_obj = o.m_obj;
    }

    // Do a deep a copy of the C++ object
    Capability& opAssign(const Capability& o)
    {
        // copy content of C++ instance
        m_obj = o.m_obj; 
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
        get const { return m_obj.m_bActive; }
        set { m_obj.m_bActive = value; }
    }
    bool m_bDidLateSetup
    {
        get { return m_obj.m_bDidLateSetup; }
        set { m_obj.m_bDidLateSetup = value; }
    }
    bool m_bDidLateSetupHello
    {
        get { return m_obj.m_bDidLateSetupHello; }
        set { m_obj.m_bDidLateSetupHello = value; }
    }
    float m_activeDuration
    {
        get { return m_obj.m_activeDuration; }
        set { m_obj.m_activeDuration = value; }
    }
    float m_inactiveDuration
    {
        get { return m_obj.m_inactiveDuration; }
        set { m_obj.m_inactiveDuration = value; }
    }
    ECapabilityTickGroup m_group
    {
        get { return m_obj.m_group; }
        set { m_obj.m_group = value; }
    }
    ECapabilityTickSubGroup m_subGroup
    {
        get { return m_obj.m_subGroup; }
        set { m_obj.m_subGroup = value; }
    }

    // Hold a reference to the C++ side of the proxy
    private Capability_t @m_obj;
};


